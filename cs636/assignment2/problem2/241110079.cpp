#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <omp.h>
#include "241110079.h" //concurrent_queue 
#include "concurrentqueue.h" //Moody camel
#include "/usr/include/boost/lockfree/queue.hpp" //Boost impl
#include "/usr/include/tbb/concurrent_queue.h" //TBB

using std::cout;
using std::endl;
using std::string;
using std::chrono::duration_cast;
using HR = std::chrono::high_resolution_clock;
using HRTimer = HR::time_point;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::filesystem::path;

static constexpr uint64_t RANDOM_SEED = 42;
int NUM_THREADS = 1;
bool moody = false, tbb1 = false, boost1 = false;

void create_file(path pth, const uint32_t* data, uint64_t size) {
  FILE* fptr = NULL;
  fptr = fopen(pth.string().c_str(), "wb+");
  fwrite(data, sizeof(uint32_t), size, fptr);
  fclose(fptr);
}

/** Read n integer data from file given by pth and fill in the output variable
    data */
void read_data(path pth, uint64_t n, uint32_t* data) {
  FILE* fptr = fopen(pth.string().c_str(), "rb");
  string fname = pth.string();
  if (!fptr) {
    string error_msg = "Unable to open file: " + fname;
    perror(error_msg.c_str());
  }
  int freadStatus = fread(data, sizeof(uint32_t), n, fptr);
  if (freadStatus == 0) {
    string error_string = "Unable to read the file " + fname;
    perror(error_string.c_str());
  }
  fclose(fptr);
}

// These variables may get overwritten after parsing the CLI arguments
/** total number of operations */
uint64_t NUM_OPS = 1e5;

// List of valid flags and description
void validFlagsDescription() {
  cout << "ops: specify total number of operations; thr: number of threads; (optional) tbb/moody/boost\n";
}

// Code snippet to parse command line flags and initialize the variables
int parse_args(char* arg) {
  string s = string(arg);
  string s1, s2;
  uint64_t val;

  try {
    size_t pos = s.find('=');
    if (pos != string::npos) {
      s1 = s.substr(0, pos);
      s2 = s.substr(pos + 1);
      val = stol(s2);
    } else {
      s1 = s;
    }
  } catch (...) {
    cout << "Supported: " << std::endl;
    cout << "-*=[], where * is:" << std::endl;
    validFlagsDescription();
    return 1;
  }

  if (s1 == "-ops") {
    NUM_OPS = val;
  } else if (s1 == "-thr") {
    NUM_THREADS = val;
  } else if (s1 == "-tbb" || s1 == "-moody" || s1 == "-boost") {
    if (s1 == "-tbb") {
      tbb1 = true;
    } else if (s1 == "-moody") {
      moody = true;
    } else if (s1 == "-boost") {
      boost1 = true;
    }
  } else {
    std::cout << "Unsupported flag: " << s1 << "\n";
    std::cout << "Use the below list of flags:\n";
    validFlagsDescription();
    return 1;
  }

  return 0;
}
// Function to run multiple push and pop operations
void performOp(int thread_id, ConcurrentQueue& Q, uint32_t key) {
    std::mt19937 gen(RANDOM_SEED+thread_id);  // Different seed per thread
    std::uniform_real_distribution<> dist(0.0, 1.0);  // Uniform distribution [0.0, 1.0]
    double prob = dist(gen);
    // cout << "prob : " << prob << std::endl;
      if (prob > 0.5) { // push
          Q.enq(key);
          // std::cout <s< "Thread " << thread_id << " queued " << key << std::endl;
      } else {
        int temp;
          int value = Q.deq(temp);  // Deq an element from the queue
          if (value) {
              // std::cout << "Thread " << thread_id << " dequeued " << value << std::endl;
          } else {
              // std::cout << "Thread " << thread_id << " tried to deq, but the queue was empty." << std::endl;
          }
      }
}

void performOp(int thread_id, moodycamel::ConcurrentQueue<uint32_t>& Q, uint32_t key) {
  std::mt19937 gen(RANDOM_SEED+thread_id);  // Different seed per thread
  std::uniform_real_distribution<> dist(0.0, 1.0);  // Uniform distribution [0.0, 1.0]
  double prob = dist(gen);
  // cout << "prob : " << prob << std::endl;
    if (prob > 0.5) { // push
        Q.enqueue(key);
        // std::cout <s< "Thread " << thread_id << " queued " << key << std::endl;
    } else {
      int temp;
        int value = Q.try_dequeue(temp);  // Deq an element from the queue
        if (value) {
            // std::cout << "Thread " << thread_id << " dequeued " << value << std::endl;
        } else {
            // std::cout << "Thread " << thread_id << " tried to deq, but the queue was empty." << std::endl;
        }
    }
}

void performOp(int thread_id, boost::lockfree::queue<uint32_t>& Q, uint32_t key) {
  std::mt19937 gen(RANDOM_SEED+thread_id);  // Different seed per thread
  std::uniform_real_distribution<> dist(0.0, 1.0);  // Uniform distribution [0.0, 1.0]
  double prob = dist(gen);
  // cout << "prob : " << prob << std::endl;
    if (prob > 0.5) { // push
        Q.push(key);
        // std::cout <s< "Thread " << thread_id << " queued " << key << std::endl;
    } else {
      uint16_t temp;
        int value = Q.pop(temp);  // Deq an element from the queue
        if (value) {
            // std::cout << "Thread " << thread_id << " dequeued " << value << std::endl;
        } else {
            // std::cout << "Thread " << thread_id << " tried to deq, but the queue was empty." << std::endl;
        }
    }
}

void performOp(int thread_id, tbb::concurrent_queue<uint32_t>& Q, uint32_t key) {
  std::mt19937 gen(RANDOM_SEED+thread_id);  // Different seed per thread
  std::uniform_real_distribution<> dist(0.0, 1.0);  // Uniform distribution [0.0, 1.0]
  double prob = dist(gen);
  // cout << "prob : " << prob << std::endl;
    if (prob > 0.5) { // push
        Q.push(key);
        // std::cout <s< "Thread " << thread_id << " queued " << key << std::endl;
    } else {
      uint32_t temp;
      Q.try_pop(temp);  // Deq an element from the queue
        if (temp != -1) {
            // std::cout << "Thread " << thread_id << " dequeued " << value << std::endl;
        } else {
            // std::cout << "Thread " << thread_id << " tried to deq, but the queue was empty." << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
  for (int i = 1; i < argc; i++) {
    int error = parse_args(argv[i]);
    if (error == 1) {
      cout << "Argument error, terminating run.\n";
      exit(EXIT_FAILURE);
    }
  }

  cout << "NUM OPS: " << NUM_OPS << "\n";

  assert(NUM_OPS > 0);

  auto* h_values_insert = new uint32_t[NUM_OPS];
  memset(h_values_insert, 0, sizeof(uint32_t) * NUM_OPS);

  // Use shared files filled with random numbers
  path cwd = std::filesystem::current_path();
  path path_insert_values = cwd / "random_values_insert.bin";

  assert(std::filesystem::exists(path_insert_values));

  // Read data from file
  auto* tmp_values_insert = new uint32_t[NUM_OPS];
  read_data(path_insert_values, NUM_OPS, tmp_values_insert);
  for (int i = 0; i < NUM_OPS; i++) {
    h_values_insert[i] = tmp_values_insert[i];
  }
  delete[] tmp_values_insert;
  // Start OpenMP parallel execution
  auto start_time = std::chrono::high_resolution_clock::now();
  if(moody){
    //Moody camel
    moodycamel::ConcurrentQueue<uint32_t> Q;
    omp_set_num_threads(NUM_THREADS);
    #pragma omp parallel for
    for(int i=0; i<NUM_OPS; i++){
      int thread_id = omp_get_thread_num();    
      performOp(thread_id, Q, h_values_insert[i]);
    } 
  }
  else if(tbb1){
    tbb::concurrent_queue<uint32_t> Q;
    omp_set_num_threads(NUM_THREADS);
    #pragma omp parallel for
    for(int i=0; i<NUM_OPS; i++){
      int thread_id = omp_get_thread_num();    
      performOp(thread_id, Q, h_values_insert[i]);
    }
  }
  else if(boost1){
    boost::lockfree::queue<uint32_t> Q(2*NUM_OPS);
    omp_set_num_threads(NUM_THREADS);
    #pragma omp parallel for
    for(int i=0; i<NUM_OPS; i++){
      int thread_id = omp_get_thread_num();    
      performOp(thread_id, Q, h_values_insert[i]);
    } 
  }
  else {
    // Create a queue
    ConcurrentQueue Q;
    omp_set_num_threads(NUM_THREADS);
    #pragma omp parallel for
    for(int i=0; i<NUM_OPS; i++){
      int thread_id = omp_get_thread_num();    
      performOp(thread_id, Q, h_values_insert[i]);
    }  

  }
    // End the timing
  auto end_time = std::chrono::high_resolution_clock::now();

  // Measure time taken for operations
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  std::cout << "Time taken for " << NUM_OPS  << " operations with "
            << NUM_THREADS << " threads: " << duration.count() << " ms" << std::endl;

  return EXIT_SUCCESS;
}
