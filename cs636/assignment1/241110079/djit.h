// #include "commonutil.h"
#include <algorithm>


void onRead_DJIT(trace_info ti){
    //t updates the relevant entry in the history of v (v is the shared location)
    std::cout << "onReadStarts:::::::::::::::::::" <<std::endl;
    std::string v = ti.address;
    int t = ti.t;
    int size = ti.size;
    for(int i=0; i<size; i++){
        // Location is not present i.e. this is the first access
        if(access_history_map.find(v) == access_history_map.end()){
            std::vector<time_frame> v_vec(max_threads);
            for(int i=0; i<max_threads; i++){
                v_vec[i].last_read = 0;
                v_vec[i].last_write = 0;
            }
            access_history_map[v] = v_vec;
            access_history_map[v][t].last_read = threads_map[t][t];
        }
        else {
            bool race_detected = false;
            //Checking for a data race i.e. we check if there exists a thread u that has also written to v such that aw_v[u] > st_t[u]
            for(int u=0; u<max_threads; u++){
                if(u!=t && access_history_map[v][u].last_write > threads_map[t][u]){
                    race_detected = true;
                    //Log an R-w data race between t and u on v
                    logDataRace(u, t, v, RW);
                }
            }
            if(!race_detected)
                access_history_map[v][t].last_read = threads_map[t][t]; //Updating only the last_read
        }
        v = add_byte(v);    
    }
    std::cout << "onReadEnds:::::::::::::::::::" <<std::endl;
}

void onWrite_DJIT(trace_info ti){
    //t updates the relevant entry in the history of v (v is the shared location)
    std::cout << "onWriteStarts:::::::::::::::::::" <<std::endl;
    std::string v = ti.address;
    int t = ti.t;
    int size = ti.size;
    for(int i=0; i<size; i++){
        // Location is not present i.e. this is the first access
        if(access_history_map.find(v) == access_history_map.end()){   
            std::vector<time_frame> v_vec(max_threads);
            for(int i=0; i<max_threads; i++){
                v_vec[i].last_read = 0;
                v_vec[i].last_write = 0;
            }        
            access_history_map[v] = v_vec;
            access_history_map[v][t].last_write = threads_map[t][t];
        }
        else {
            bool race_detected = false;
            // Checking for a data race i.e. we check if there exists a thread u that has also written to v such that aw_v[u] > st_t[u]
            for(int u=0; u<max_threads; u++){
                if(u!=t && access_history_map[v][u].last_write > threads_map[t][u]){
                    //Log a W-W data race between t and u on v
                    race_detected = true; 
                    logDataRace(u, t, v, WW);
                }
            if(u!=t && access_history_map[v][u].last_read > threads_map[t][u]){
                    //Log a W-R data race between t and u on v
                    race_detected = true; 
                    logDataRace(u, t, v, WR);
                }
            }
            if(!race_detected)
                access_history_map[v][t].last_write = threads_map[t][t]; //Updating only the last_write
        }
        v = add_byte(v);    
    }
    std::cout << "onWriteEnds:::::::::::::::::::" <<std::endl;
 
}

void djit(){
    std::ifstream file(file_path);
    std::string line;
    std::smatch match;
    while (std::getline(file, line)) {
        int parent_value = isBeforeCreate(line);
        int forked_thread = isFork(line);
        int joined_thread = isJoin(line);
        //If a thread has perfomed a fork operation (indicated in the Parent value of pthread_create), push in the stack
        if(parent_value >= 0){
            S.push(parent_value);
        }
        //Thread begin: xx (Indicates a fork) : Child will inherit parent's vector clock
        else if(isFork(line) >= 0){
            //peek from stack
            //take union of vector clocks
            //increment parent
            if(!S.empty()){
                int t = S.top();
                for(int i=0; i<max_threads; i++){
                    threads_map[forked_thread][i] = std::max(threads_map[forked_thread][i], threads_map[t][i]);
                }
                threads_map[t][t]++;
            } 

        }
        //Thread ended: xx (Indicates a join) : Parent thread updates its own clock wrt child thread
        else if(isJoin(line) >= 0){
            //pop from stack
            //take union of vector clocks
            //increment child
            if(!S.empty()){
                int t = S.top();
                S.pop();
                for(int i=0; i<max_threads; i++){
                    threads_map[t][i] = std::max(threads_map[joined_thread][i], threads_map[t][i]);
                }
                threads_map[joined_thread][joined_thread]++;
            }
        }
        else if(isMemoryAccess(line)){
            trace_info ti = parseLine(line);
            if(locks_map.find(ti.address) != locks_map.end()){
                //Lock variables do not race by definition, so we skip those.
                continue;
            }
            //Is a read operatiom
            if(ti.isRead){
                onRead_DJIT(ti);
            }
            //Is a write operation
            else {
                onWrite_DJIT(ti);
            }
        }
        //Is a release operation
        else if(isLockRelease(line)){
            std::regex_search(line, match, TID);
            int t = std::stoi(match[1]);
            std::regex_search(line, match, LOCKADDRESS);
            std::string lock = match[1].str();
            onRelease(t, lock);
        }
        //Is an acquire operation
        else if(isLockAcquire(line)){
            std::regex_search(line, match, TID);
            int t = std::stoi(match[1]);
            std::regex_search(line, match, LOCKADDRESS);
            std::string lock = match[1].str();
            onAcquire(t, lock);
        }

    }
    file.close(); 
}
