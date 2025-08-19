#include<iostream>
#include<pthread.h>
#include<unistd.h>
#include<queue>
#include<string>
#include<fstream>
#include<atomic>
#include<omp.h>
using std::atomic;
using namespace std;
string R,W;
int T,L,M;
atomic<int> START_READING_FROM, END_OF_FILE_REACHED;
atomic<int> current_thread, current_line_count;
ofstream output_file;
ifstream input_file;
queue<string> buffer;


//Write function for the consumer thread
void write_to_file(int thread_id){
    // printf("Thread %d (Consumer) trying to get into critical region.\n", thread_id);
    if(END_OF_FILE_REACHED.load() == 1 && buffer.empty()){
        /*
        If the file has been read completely, there is no reason for this thread to stay in the loop.
        */
        printf("Consumer thread is exiting as the end of file has been reached and buffer has been emptied.\n");
        return;
    }
    #pragma omp critical
    {
        printf("------------------------------------------------------------\n");
        printf("Thread %d (Consumer) is in critical region.\n", thread_id);
        if(!buffer.empty()){
            output_file << buffer.front() << endl;
            buffer.pop();
            printf("Thread %d (consumer) has removed an item from the buffer Q%d.\n", thread_id, (int)buffer.size());
        }
        cout << "Consumer thread exiting critical region.\n" <<endl;
        printf("------------------------------------------------------------\n");
    }
    return;
}

//Read function for the producer threads
void read_from_file(int thread_id){
    // printf("Thread %d is starting.\n", thread_id);
    if(END_OF_FILE_REACHED.load() == 1){
        /*
        If the file has been read completely, there is no reason for this thread to stay in the loop.
        */
        printf("Thread %d is exiting as file has ended!!!\n", thread_id);
        return;
    }
    #pragma omp critical
    {
        /*Setting the thread responsible for reading L lines*/
        if(current_thread.load() == -1 && current_line_count.load() == 0){
            current_thread.store(thread_id);
            printf("The current thread for reading is Thread %d.\n", thread_id);
        }
    }
    if(buffer.size() == M){
        return;
    }
    if(current_thread.load() != thread_id){
        printf("Thread %d is exiting as it is not responsible for reading yet.\n", thread_id);
        return;
    }
    if(current_line_count.load() == L){
        /*If a thread has completed reading its share of lines then it resets line count and current thread id */
        current_line_count.store(0);
        current_thread.store(-1);
        printf("Thread %d is exiting as it has completed its share.\n", thread_id);
        return;
    }
    string line;
    if(current_line_count.load() < L){
        if(buffer.size()<M){
            if(!input_file.eof()){
                #pragma omp critical
                {
                    printf("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
                    printf("Thread %d (Producer) is in critical region.\n", thread_id);
                    getline(input_file,line);
                    buffer.push(line);
                    printf( "[T%d, Q%d]\n", thread_id, (int)buffer.size());
                    current_line_count.fetch_add(1);
                    printf("Thread %d has read %d lines\n", thread_id, current_line_count.load());  
                    cout << "Thread " << thread_id << " exiting critical region.\n" <<endl;
                    printf("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
                }
            }
            else{
                cout << "Reached the end of file.\n";
                #pragma omp critical
                {   
                    if(END_OF_FILE_REACHED.load() == 0)
                        END_OF_FILE_REACHED.store(1);
                }
            }
        }
    }
    if(END_OF_FILE_REACHED.load()== 0)
        START_READING_FROM.store(input_file.tellg());
    return;
}
int main(int argc, char** argv){
    if(argc < 5){
        cout << "Requires 5 arguments." <<endl;
        exit(1);
    }

    /*
        Initializing input parameters and other global variables 
        * R = Input File Location
        * T = #Producer threads
        * L = #Lines to be read by each thread
        * M = Size of Shared Buffer
        * W = Output File Location
    */
    R = argv[1];
    T = atoi(argv[2]);
    L = atoi(argv[3]);
    M = atoi(argv[4]);
    W = argv[5];
    START_READING_FROM.store(0);
    END_OF_FILE_REACHED.store(0);
    current_line_count.store(0);
    current_thread.store(-1);
    input_file.open(R);
    if(!input_file.is_open()){
        cout << "Cannot open input file." << endl;
        exit(1);
    }
    output_file.open(W);
    if(!output_file.is_open()){
        cout << "Cannot open output file." << endl;
        exit(1);
    }
    int tid;
    omp_set_num_threads(T+1);
    while(END_OF_FILE_REACHED.load() == 0 || !buffer.empty()){
        #pragma omp parallel 
        {
            tid = omp_get_thread_num();
            #pragma omp task
            {
                if(tid!=T)
                    read_from_file(tid);
            }

            #pragma omp task
            {
                if(tid == T)
                    write_to_file(tid);
            }               
        }
    }
    cout << "All producer threads have completed." <<endl;
    input_file.close();
    output_file.close();


    return 0;
}