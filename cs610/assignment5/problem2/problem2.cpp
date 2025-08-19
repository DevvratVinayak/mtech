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
#include "concurrent_stack.h"

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
const int NUM_THREADS = 16;

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
uint64_t NUM_OPS = 1e8;

// List of valid flags and description
void validFlagsDescription() {
  cout << "ops: specify total number of operations\n";
}

// Code snippet to parse command line flags and initialize the variables
int parse_args(char* arg) {
  string s = string(arg);
  string s1;
  uint64_t val;

  try {
    s1 = s.substr(0, 4);
    string s2 = s.substr(5);
    val = stol(s2);
  } catch (...) {
    cout << "Supported: " << std::endl;
    cout << "-*=[], where * is:" << std::endl;
    validFlagsDescription();
    return 1;
  }

  if (s1 == "-ops") {
    NUM_OPS = val;
  } else {
    std::cout << "Unsupported flag:" << s1 << "\n";
    std::cout << "Use the below list flags:\n";
    validFlagsDescription();
    return 1;
  }
  return 0;
}

// Function to run multiple push and pop operations
void performOp(int thread_id, ConcurrentStack& stack, uint32_t key) {
    std::mt19937 gen(RANDOM_SEED);  // Different seed per thread
    std::uniform_real_distribution<> dist(0.0, 1.0);  // Uniform distribution [0.0, 1.0]
    double prob = dist(gen);
      if (prob >= 0.5) { // push
          stack.push(key);
          std::cout << "Thread " << thread_id << " pushed " << key << std::endl;
      } else {
          int popped_value = stack.pop();  // Pop an element from the stack
          if (popped_value != -1) {
              std::cout << "Thread " << thread_id << " popped " << popped_value << std::endl;
          } else {
              std::cout << "Thread " << thread_id << " tried to pop, but the stack was empty." << std::endl;
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

  // Create a stack
  ConcurrentStack stack;

  // Calculate the number of operations per thread
  uint64_t num_ops_per_thread = NUM_OPS / NUM_THREADS;
  uint64_t remainder = NUM_OPS % NUM_THREADS;

  // Start OpenMP parallel execution
  auto start_time = std::chrono::high_resolution_clock::now();
  omp_set_num_threads(NUM_THREADS);
  #pragma omp parallel for
  for(int i=0; i<NUM_OPS; i++){
    int thread_id = omp_get_thread_num();    
    performOp(thread_id, stack, h_values_insert[i]);
  }

  // End the timing
  auto end_time = std::chrono::high_resolution_clock::now();

  // Measure time taken for operations
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  std::cout << "Time taken for " << NUM_OPS  << " operations with "
            << NUM_THREADS << " threads: " << duration.count() << " ms" << std::endl;

  return EXIT_SUCCESS;
}
