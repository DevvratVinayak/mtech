// Coarse-grained locking implies 1 lock for the whole map
// Fine-grained locking implies 1 lock for each key in the map, which is
// encouraged

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <pthread.h>
#include <queue>
#include <string>
#include <unistd.h>
#include<chrono>
#include <atomic> 
using namespace std::chrono;

using HR = high_resolution_clock;
using HRTimer = HR::time_point;
using std::cerr;
using std::cout;
using std::endl;
using std::ios;
using std::atomic;

// Max different files
const int MAX_FILES = 10;
const int MAX_SIZE = 10;
int MAX_THREADS = 5;
const int CACHE_LINE_SIZE=64;

struct t_data {
  uint32_t tid;
};

// struct to keep track of the number of occurrences of a word
//Adding padding such that Each variable has its own block
//removing mutex as updates casn be done atomically so true sharing dependency is resolved.
struct word_tracker {
  atomic<uint64_t> word_count[4][8];
  atomic<uint64_t> total_lines_processed;
  char padding2[CACHE_LINE_SIZE-sizeof(uint64_t)];
  atomic<uint64_t> total_words_processed;
  char padding3[CACHE_LINE_SIZE-sizeof(uint64_t)];
} tracker;

// Shared queue, to be read by producers
std::queue<std::string> shared_pq;
// updates to shared queue
pthread_mutex_t pq_mutex = PTHREAD_MUTEX_INITIALIZER;

// lock var to update to total line counter
pthread_mutex_t line_count_mutex = PTHREAD_MUTEX_INITIALIZER;

// each thread read a file and put the tokens in line into std out
void *thread_runner(void *);

void print_usage(char *prog_name) {
  cerr << "usage: " << prog_name << " <producer count> <input file>\n";
  exit(EXIT_FAILURE);
}

void print_counters() {
  for (int id = 0; id < MAX_THREADS; ++id) {
    std::cout << "Thread " << id << " counter: " << tracker.word_count[id][0].load()
              << '\n';
  }
}

void fill_producer_buffer(std::string &input) {
  std::fstream input_file;
  input_file.open(input, ios::in);
  if (!input_file.is_open()) {
    cerr << "Error opening the top-level input file!" << endl;
    exit(EXIT_FAILURE);
  }

  std::filesystem::path p(input);
  std::string line;
  while (getline(input_file, line)) {
    shared_pq.push(p.parent_path() / line);
  }
}

int thread_count = 0;

int main(int argc, char *argv[]) {
  if (argc != 3) {
    print_usage(argv[0]);
  }

  thread_count = strtol(argv[1], NULL, 10);
  MAX_THREADS = thread_count;
  std::string input = argv[2];
  fill_producer_buffer(input);

  pthread_t threads_worker[thread_count];

  int file_count;

  struct t_data *args_array =
      (struct t_data *)malloc(sizeof(struct t_data) * thread_count);
  for (int i = 0; i < thread_count; i++)
    tracker.word_count[i][0].store(0);
  tracker.total_lines_processed.store(0);
  // tracker.word_count_mutex = PTHREAD_MUTEX_INITIALIZER;

  for (int i = 0; i < thread_count; i++) {
    args_array[i].tid = i;
    pthread_create(&threads_worker[i], nullptr, thread_runner,
                   (void *)&args_array[i]);
  }
  HRTimer start = HR::now();
  for (int i = 0; i < thread_count; i++)
    pthread_join(threads_worker[i], NULL);
  HRTimer end = HR::now();
  auto duration = duration_cast<microseconds>(end - start).count();
  print_counters();
  cout << "Total words processed: " << tracker.total_words_processed.load() << "\n";
  cout << "Total line processed: " << tracker.total_lines_processed.load() << "\n";
  cout << "Time taken (us): " << duration << "\n";

  return EXIT_SUCCESS;
}

// TODO: inefficient counting of total words
void *thread_runner(void *th_args) {
  struct t_data *args = (struct t_data *)th_args;
  uint32_t thread_id = args->tid;
  std::fstream input_file;
  std::string fileName;
  std::string line;

  pthread_mutex_lock(&pq_mutex);
  fileName = shared_pq.front();
  shared_pq.pop();
  pthread_mutex_unlock(&pq_mutex);

  input_file.open(fileName.c_str(), ios::in);
  if (!input_file.is_open()) {
    cerr << "Error opening input file from a thread!" << endl;
    exit(EXIT_FAILURE);
  }

  while (getline(input_file, line)) {
    tracker.total_lines_processed.fetch_add(1);
    std::string delimiter = " ";
    size_t pos = 0;
    std::string token;
    /*Executing the entire loop atomically to avoid unnecessary misses*/
    while ((pos = line.find(delimiter)) != std::string::npos) {
      token = line.substr(0, pos);
      tracker.word_count[thread_id][0].fetch_add(1);
      tracker.total_words_processed.fetch_add(1);
      line.erase(0, pos + delimiter.length());
    }
  }

  input_file.close();

  pthread_exit(nullptr);
}
