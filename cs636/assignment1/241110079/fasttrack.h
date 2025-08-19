// #include "commonutil.h"
#include <algorithm>
void onRead_FT(trace_info ti){
    int t = ti.t;
    std::string v = ti.address;
    int size = ti.size;
    uint64_t thread_time;
    uint64_t last_write = 0;
    for(int i=0; i<size; i++){
        if(write_epochs.find(v) != write_epochs.end()){
            last_write = write_epochs[v].time;
        }
        //If read_x is stored as an epoch : 3 cases arise
        if(read_epochs.find(v) != read_epochs.end()){
            uint64_t last_read = read_epochs[v].time;
            int u = read_epochs[v].id;
            thread_time = threads_map[t][u];
            if(last_write <= thread_time){
                if(last_read < thread_time){
                    if(u == t){
                        //FT READ SAME EPOCH
                        read_epochs[v].time = threads_map[t][t];
                    }
                    else {
                        //FT READ EXCLUSIVE
                        read_epochs[v].time = threads_map[t][t];
                        read_epochs[v].id = t;
                    }
                }
                else if(u != t) {
                    //There are concurrent threads trying to read, so we promote read epoch to vector clock
                    //FT READ SHARE 
                    // Current epoch : c@u
                    std::vector<uint64_t> V(max_threads, 0);
                    //V = {0}
                    //V[t] = C_t[t]; V[u] = c  
                    V[t] = threads_map[t][t];
                    V[u] = thread_time;
                    //Add the vector to the reads and remove from epochs list
                    read_map[v] = V;
                    read_epochs.erase(v);
                }
            }
            else {
                //Log R-W conflict t,u : u = write_epochs[v].id
                logDataRace(write_epochs[v].id, t, v, RW);
            }

        }
        //If read_x is a vector clock : FT READ SHARED
        else if(read_map.find(v) != read_map.end()){
            if(last_write <= thread_time){
                read_map[v][t] = thread_time;
            }
            else {
                //Log RW conflict t,u : u = write_epochs[v].id
                logDataRace(write_epochs[v].id, t, v, RW);
            }
        }
        //x has not been read yet
        else {
            epoch e;
            e.id = t;
            e.time = threads_map[t][t];
            read_epochs[v] = e;
        }
        v = add_byte(v);
    }
    
}

void onWrite_FT(trace_info ti){
    int t = ti.t;
    std::string v = ti.address;
    int size = ti.size;
    uint64_t thread_time;
    uint64_t last_write = 0;
    for(int i=0; i<size; i++){
        if(write_epochs.find(v) == write_epochs.end()){
            //First write
            epoch e;
            e.id = t;
            e.time = threads_map[t][t];
            write_epochs[v] = e; 
        }
        else if(write_epochs[v].id == t){
            //FT WRITE SAME EPOCH
            if(write_epochs[v].time < threads_map[t][t] ){
                write_epochs[v].time = threads_map[t][t];
            } 
        }
        else {
            last_write = write_epochs[v].time; //c
            int u = write_epochs[v].id; //u; W_x = c@u : Last write epoch
            thread_time = threads_map[t][u]; //C_t
            if(last_write <= thread_time){
                if(read_epochs.find(v) != read_epochs.end()){
                    //FT WRITE EXCLUSIVE
                    if(read_epochs[v].time <= thread_time){
                        write_epochs[v].time = threads_map[t][t];
                        write_epochs[v].id = t;
                    }
                    else{
                        //Log WR data race
                        logDataRace(t, read_epochs[v].id, v, WR);
                    }
                }
                else if(read_map.find(v) != read_map.end()) {
                    //R_x is a vector clock
                    //FT WRITE SHARED
                    bool happens_before = true;
                    for(int j=0; j<max_threads; j++){
                        if(read_map[v][j] > threads_map[t][j]){
                            happens_before = false;
                            break;
                        }
                    }
                    if(happens_before){
                        write_epochs[v].time = threads_map[t][t];
                        write_epochs[v].id = t;
                        
                        //Since writes are overwritten we demote the read VC to read epoch (It gets intialized on a read, so deleting from read_map is sufficient)
                        read_map.erase(v);
                    }
                    else {
                        // Log WR data race
                        logDataRace(t, read_epochs[v].id, v, WR);
                    }
                }
                else {
                    //Either a blind write or reads may have been reset after write
                    write_epochs[v].time = threads_map[t][t];
                    write_epochs[v].id = t;
                }
            }
            else {
                //Log WW data race
                logDataRace(write_epochs[v].id, t, v, WW);
            }
        }
        v = add_byte(v);
    }
}

void fasttrack(){
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
        else if(isFork(line) >= 0 ){
            //peek from stack
            //take union of vector clocks
            //increment parent
            if(!S.empty()){ //An empty stack indicates no child thread has been spawned yet.
                int t = S.top();
                for(int i=0; i<max_threads; i++){
                    threads_map[forked_thread][i] = std::max(threads_map[forked_thread][i], threads_map[t][i]);
                }
                threads_map[t][t]++;
            } 

        }
        //Thread ended: xx (Indicates a join) : Parent thread updates its own clock wrt child thread
        else if(isJoin(line) >= 0 ){
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
                onRead_FT(ti);
            }
            //Is a write operation
            else {
                onWrite_FT(ti);
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
