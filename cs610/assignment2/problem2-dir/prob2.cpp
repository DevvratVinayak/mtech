#include<iostream>
#include<pthread.h>
#include<unistd.h>
#include<queue>
#include<string>
#include<fstream>
#include<atomic>
using std::atomic;
using namespace std;
string R,W;
int T,L,M;
atomic<int> START_READING_FROM, END_OF_FILE_REACHED;
ofstream output_file;
ifstream input_file;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t exclusive_lock = PTHREAD_MUTEX_INITIALIZER;
queue<string> buffer;
pthread_cond_t buffer_is_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t buffer_is_empty = PTHREAD_COND_INITIALIZER;


//Write function for the consumer thread
void* write_to_file(void* arg){
    while(1){
        pthread_mutex_lock(&lock);
        /*Consumer thread must wait in case the buffer is empty.
        */
        if(buffer.size() == 0 && END_OF_FILE_REACHED.load()==0)
            pthread_cond_wait(&buffer_is_full, &lock);
        if(buffer.size() == 0 && END_OF_FILE_REACHED.load()==1){
            /*
            *The consumer need not enter the critical section once all the producer threads have completed,
            *So, it releases the mutex and breaks out of the loop.
            */
            pthread_mutex_unlock(&lock);
            break;
        }

        /*Consumer thread needs to empty the buffer all at once*/

        while(!buffer.empty()){
            output_file << buffer.front() << endl;
            buffer.pop();
        }
        printf("Consumer Thread has emptied the buffer.\n");
        pthread_cond_signal(&buffer_is_empty);
        pthread_mutex_unlock(&lock);       
    }
    cout << "Exiting consumer thread." <<endl;
    pthread_exit(0);
    return NULL;
}

//Read function for the producer threads
void* read_from_file(void* arg){
    int *thread_ptr = (int*)arg;
    int thread_id = *thread_ptr;
    while(1){
        pthread_mutex_lock(&exclusive_lock);
        pthread_mutex_lock(&lock);
        printf("Thread %d is starting.\n", thread_id);
        /*
        A thread needs to wait if the buffer os full before adding lines to it.
        */
        while(buffer.size() == M)
            pthread_cond_wait(&buffer_is_empty, &lock);
        if(END_OF_FILE_REACHED.load() == 1){
            /*
            If the file has been read completely, there is no reason for this thread to stay in the loop.
            */
            printf("Thread %d is exiting.\n", thread_id);
            pthread_mutex_unlock(&lock);
            pthread_mutex_unlock(&exclusive_lock);
            break;
        }
        string line;
        atomic<int> number_of_lines_read;
        number_of_lines_read.store(0);
        printf("Size of buffer Before: %d\n", (int)buffer.size() );
        while(number_of_lines_read.load() < L){
            if(!input_file.eof()){
                getline(input_file,line);
                buffer.push(line);
                printf( "[T%d, Q%d]\n", thread_id, (int)buffer.size());
                number_of_lines_read.fetch_add(1);
                if(buffer.size() == M){
                    printf("Buffer Got Full\n");
                    pthread_cond_signal(&buffer_is_full);
                    pthread_mutex_unlock(&lock);
                    pthread_cond_wait(&buffer_is_empty,&lock);                    
                }
            }
            else{
                cout << "Reached the end of file. "<< endl;
                END_OF_FILE_REACHED.store(1);
                break;
            }
        }
        printf("Thread %d has read %d lines\n", thread_id, number_of_lines_read.load());
        printf("Size of buffer After : %d\n", (int)buffer.size() );
        printf("-------------------------------------------------------\n");
        if(END_OF_FILE_REACHED.load()== 0)
            START_READING_FROM.store(input_file.tellg());
        pthread_mutex_unlock(&lock);
        pthread_mutex_unlock(&exclusive_lock);
        if(END_OF_FILE_REACHED.load() == 1){
            pthread_cond_signal(&buffer_is_full);
        }
        sleep(1);
    }
    pthread_exit(0);
    return NULL;
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
    pthread_t consumer;
    pthread_t producers[T];
    int thread_numbers[T];

    for(int i=0; i<T;i++){
        thread_numbers[i] = i+1;
        pthread_create(&producers[i], NULL, read_from_file, &thread_numbers[i]);
    }

    pthread_create(&consumer, NULL, write_to_file, NULL);

    for(int i=0; i<T; i++){
        pthread_join(producers[i], NULL);
    }
    
    cout << "All producer threads have completed." <<endl;
    pthread_join(consumer, NULL);

    input_file.close();
    output_file.close();

    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&buffer_is_empty);
    pthread_cond_destroy(&buffer_is_full);

    return 0;
}
