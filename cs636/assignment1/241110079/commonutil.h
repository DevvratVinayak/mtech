#ifndef COMMONUTIL_H
#define COMMONUTIL_H

#include <unordered_map>
#include <string>
#include <vector>
#include "constants.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stack>

//Constants that require initialisation
int max_threads = 0;

//Stack that stores the parent thread ids
std::stack<int> S;

std::string file_path;

struct time_frame {
    uint64_t last_read;
    uint64_t last_write;
};
struct data_race {
    uint64_t count; 
    int thread1, thread2;
    DataRaceType type;
};
struct trace_info {
    std::string address;
    int t;
    bool isRead;
    int size;
};

struct epoch {
    int id;
    uint64_t time;
};

//Common data structures for both algorithms
std::unordered_map<std::string, std::vector<data_race>> data_race_map; //This represents a collection of data races of all the shared locations
std::unordered_map<std::string, std::vector<uint64_t>> locks_map; //This represents a collection of vector clocks as maintained by the locks
std::unordered_map<int, std::vector<uint64_t>> threads_map; //This represents a collection of vector clocks as maintained by the threads.

//For DJIT+
std::unordered_map<std::string, std::vector<time_frame>> access_history_map; //This represents a collection of access histories of all the shared locations


//For FastTrack
std::unordered_map<std::string, epoch> read_epochs; //This represents a collection of read epochs of all the shared locations
std::unordered_map<std::string, epoch> write_epochs; //This represents a collection of write epochs of all the shared locations
std::unordered_map<std::string, std::vector<uint64_t>> read_map; //This is maintained for each memory location if reads to a location are concurrent 

/* Functions common to both algorithms ::::::::::::::::::::Start:::::::::::::::::::: */
trace_info parseLine(std::string line){
    trace_info result;
    std::smatch match;

    // Extract TID
    if (std::regex_search(line, match, TID)) {
        result.t = std::stoi(match[1]);
    }

    // Extract ADDR
    if (std::regex_search(line, match, ADDR)) {
        result.address = match[1];
    }

    // Extract isRead
    if (std::regex_search(line, match, ISREAD)) {
        result.isRead = std::stoi(match[1]) == 1 ? true : false;
    }

    //Extract size
    if (std::regex_search(line, match, SIZE)) {
        result.size = std::stoi(match[1]) ;
    }

    return result;
}

std::string add_byte(const std::string& hexStr) {
    std::string hexValue = hexStr.substr(2); 
    unsigned long long num;
    std::stringstream ss;
    ss << std::hex << hexValue;
    ss >> num;
    num += 0x01;
    std::stringstream result;
    result << "0x" << std::setw(hexValue.length()) << std::setfill('0') << std::hex << num;

    return result.str();
}
bool isMemoryAccess(std::string line){
    if(std::regex_search(line, TID_BEGIN))
        return true;
    return false;
}

bool isLockRelease(std::string line){
    if(std::regex_search(line, AFTER_LOCK_RELEASE)){
        // std::cout << "Line : " << line <<std::endl;
        return true;
    }
    return false;
}

bool isLockAcquire(std::string line){
    if(std::regex_search(line, AFTER_LOCK_ACQUIRE))
        return true;
    return false;
}

//gets the id of the parent (if indicated in the line) returns -1 otherwise.
int isBeforeCreate(std::string line){
    std::smatch match;
    int parent_value = -1;
    if (std::regex_search(line, match, PARENT)) {
        parent_value = std::stoi(match[1].str());
    } 
    return parent_value;
}

//gets the id of the forked thread (if indicated in the line) returns -1 otherwise.
int isFork(std::string line){
    std::smatch match;
    int tid = -1;
    if (std::regex_search(line, match, THREAD_BEGIN)) {
        tid = std::stoi(match[1].str());
    }
    return tid; 
}

//gets the id of the joined thread (if indicated in the line) returns -1 otherwise.
int isJoin(std::string line){
    std::smatch match;
    int tid = -1;
    if (std::regex_search(line, match, THREAD_ENDED)) {
        tid = std::stoi(match[1].str());
    }
    return tid;
}

void onRelease(int t, std::string lock){

    //DJIT : Each entry in lock (s) is updated to hold the max of current value and that of t's vector i.e. for all i, st_s[i] <- max(st_t[i], st_s[i])
    //FastTrack : L' = L[m := C_t]
    for(int i=0; i<max_threads; i++)
        locks_map[lock][i] = std::max(threads_map[t][i], locks_map[lock][i]);
    //DJIT : Issuing thread t starts a new time frame i.e. st_t[t] <- st_t[t] + 1
    //FastTrack : C' = C[t := inc_t(C_t)] 
    threads_map[t][t]++;
}

void onAcquire(int t, std::string lock){
    //DJIT : Issuing thread t updates each entry in its vector to hold the max of current value and that of lock's (s) 
    // vector i.e. for all i, st_t[i] <- max(st_t[i], st_s[i])
    //FastTrack : C' = C[ t:= (C_t U L_m)] 
    for(int i=0; i<max_threads; i++)
        threads_map[t][i] = std::max(threads_map[t][i], locks_map[lock][i]);

}

void getMaxThreads() {
    std::ifstream file(file_path);
    std::string line;
    std::string lock;
    std::smatch match;
    // int count = 0;
    while (std::getline(file, line)) {
        if(isFork(line) >= 0) 
            max_threads++;
        // If a lock address is found, add it to the set
        else if (std::regex_search(line, match, LOCKADDRESS)) {
            lock = match[1].str();
            if(locks_map.find(lock) == locks_map.end()){
                locks_map[lock] = std::vector<uint64_t>();
            }
        }
        // count++;
    }
    file.close();
    if (max_threads == 0) {
        std::cerr << "No thread IDs found in the log file." << std::endl;
    }
}

void initializeThreadsMap(){
     int i;
    for(i=0; i<max_threads; i++){
        threads_map[i] = std::vector<uint64_t>(max_threads, 1); //For all i, st_t[i] <- 1
    }
}

void initializeLocksMap(){
    for(const auto& pair : locks_map){
        locks_map[pair.first] = std::vector<uint64_t>(max_threads, 0); // For all i, st_s[i] <- 0 
    }
    
}

void initialize() {
    getMaxThreads();
    initializeThreadsMap();
    if (locks_map.size() == 0) {
        std::cerr << "No lock variables in the log file." << std::endl;
    }
    else 
        initializeLocksMap();
}

void logDataRace(int t, int u, std::string v, DataRaceType type){
    std::vector<data_race> vec;
    if(data_race_map.find(v) == data_race_map.end()){
        //First data race on the location v
        data_race dr = { 1, t, u, type };
        vec.push_back(dr);
        data_race_map[v] = vec;  
    }
    else {
        bool new_race = true;
      for(int i=0; i<data_race_map[v].size(); i++){
        if(data_race_map[v][i].thread1 == t && data_race_map[v][i].thread2 == u && data_race_map[v][i].type == type){
            data_race_map[v][i].count++;
            new_race =false;
        }
      }
      if(new_race){
            data_race dr = { 1, t, u, type };
            data_race_map[v].push_back(dr);
      }  
    }
}

void printDataRaces(){
    if(data_race_map.empty()){
        std::cout << "NO DATA RACES FOUND IN THIS TRACE." << std::endl;
        return;
    }
    for(const auto& pair : data_race_map){
        std::vector<data_race> vec = pair.second;
        for(int i=0; i<vec.size(); i++){
            std::cout << pair.first << " " << dr_strings[vec[i].type] << " TID: " 
                    << vec[i].thread1 << " TID: " << vec[i].thread2 
                    << " Count: " << vec[i].count << std::endl;  
        }
    }
}

/* Functions common to both algorithms ::::::::::::::::::::End:::::::::::::::::::: */

#endif // COMMONUTIL_H
