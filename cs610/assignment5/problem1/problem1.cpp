#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include "hash_table.h"
#include "/usr/include/tbb/concurrent_hash_map.h"
#include <omp.h>


using std::cout;
using std::endl;
using std::string;
using std::chrono::duration_cast;
using HR = std::chrono::high_resolution_clock;
using HRTimer = HR::time_point;
using std::chrono::microseconds;
using std::chrono::milliseconds;
using std::filesystem::path;
using namespace tbb;

typedef struct {
  uint32_t key;
  uint32_t value;
} KeyValue;


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
uint64_t NUM_OPS = 100;
/** percentage of insert queries */
uint64_t INSERT = 100;
/** percentage of delete queries */
uint64_t DELETE = 0;
/** number of iterations */
uint64_t runs = 2;

bool TBB = false;

// List of valid flags and description
void validFlagsDescription() {
  cout << "ops: specify total number of operations\n";
  cout << "rns: the number of iterations\n";
  cout << "add: percentage of insert queries\n";
  cout << "rem: percentage of delete queries\n";
}

// Code snippet to parse command line flags and initialize the variables
int parse_args(char* arg) {
  string s = string(arg);
  string s1;
  uint64_t val;

  try {
    s1 = s.substr(0, 4);
    // cout << s1 <<endl;
    string s2 = s.substr(5);
    // cout << s2 << endl;
    val = stol(s2);
  } catch (...) {
    cout << "Supported: " << std::endl;
    cout << "-*=[], where * is:" << std::endl;
    validFlagsDescription();
    return 1;
  }

  if (s1 == "-ops") {
    NUM_OPS = val;
  } else if (s1 == "-rns") {
    runs = val;
  } else if (s1 == "-add") {
    INSERT = val;
  } else if (s1 == "-rem") {
    DELETE = val;
  } else if(s1 == "-tbb"){
    TBB = val == 1 ? true : false;
  }else {
    std::cout << "Unsupported flag:" << s1 << "\n";
    std::cout << "Use the below list flags:\n";
    validFlagsDescription();
    return 1;
  }
  return 0;
}

void batch_insert(HashTable* ht, KeyValue* kv_pairs, bool* result, size_t batchSize, size_t numThreads) {
    // Set number of threads for OpenMP
    omp_set_num_threads(numThreads);
    // Parallelize the insertion of key-value pairs using OpenMP
    #pragma omp parallel for
    for (size_t i = 0; i < batchSize; ++i) {
        result[i] = insert(ht, kv_pairs[i].key, kv_pairs[i].value);  // Insert key and value separately
    }
}

void batch_delete(HashTable* ht, uint32_t* keys, bool* result, size_t batchSize, size_t numThreads) {
    omp_set_num_threads(numThreads);   
    // Parallelize the insertion of key-value pairs using OpenMP
    #pragma omp parallel for
    for (size_t i = 0; i < batchSize; ++i) {
        // printf("%d::::::::::::::::\n", lookup(ht, 20));
        result[i] = deleteEntry(ht, keys[i]);  
    }
}


void batch_lookup(HashTable* ht, uint32_t* keys, uint32_t* result, size_t batchSize, size_t numThreads) {
    omp_set_num_threads(numThreads);
    
    // Parallelize the insertion of key-value pairs using OpenMP
    #pragma omp parallel for
    for (size_t i = 0; i < batchSize; ++i) {
        // printf("%d::::::::::::::::\n", lookup(ht, 7723));
        result[i] = lookup(ht, keys[i]); 
    }

}

// Function to perform batch insertions using OpenMP
void batch_insert_TBB(concurrent_hash_map<uint32_t, uint32_t>* ht, KeyValue* kv_pairs, bool* result, size_t batchSize, size_t numThreads) {
    omp_set_num_threads(numThreads);
    
    #pragma omp parallel for
    for (size_t i = 0; i < batchSize; ++i) {
        result[i] = ht->insert({kv_pairs[i].key, kv_pairs[i].value});  
    }
}

void batch_delete_TBB(concurrent_hash_map<uint32_t, uint32_t>* ht, uint32_t* keys, bool* result, size_t batchSize, size_t numThreads) {
    omp_set_num_threads(numThreads);   
    #pragma omp parallel for
    for (size_t i = 0; i < batchSize; ++i) {
        result[i] = ht->erase(i); 
    }
}


void batch_lookup_TBB(concurrent_hash_map<uint32_t, uint32_t>* ht, uint32_t* keys, uint32_t* result, size_t batchSize, size_t numThreads) {
    omp_set_num_threads(numThreads);
    concurrent_hash_map<uint32_t, uint32_t>::const_accessor accessor;
    #pragma omp parallel for
    for (size_t i = 0; i < batchSize; ++i) {
        if(ht->find(accessor, keys[i]))
          result[i] = accessor->second;
        else  
          result[i] = SENTINEL_VALUE;
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

  uint64_t ADD = NUM_OPS * (INSERT / 100.0);
  uint64_t REM = NUM_OPS * (DELETE / 100.0);
  uint64_t FIND =  NUM_OPS - (ADD+REM);

  cout << "NUM OPS: " << NUM_OPS << " ADD: " << ADD << " REM: " << REM
       << " FIND: " << FIND << "\n";

  assert(ADD > 0);

  auto* h_kvs_insert = new KeyValue[ADD];
  auto* result_insert = new bool[ADD];
  memset(result_insert, 0, sizeof(bool) * ADD);
  memset(h_kvs_insert, 0, sizeof(KeyValue) * ADD);
  auto* h_keys_del = new uint32_t[REM];
  auto* result_del = new bool[REM];
  memset(result_del, 0, sizeof(bool) * REM);
  memset(h_keys_del, 0, sizeof(uint32_t) * REM);
  auto* h_keys_lookup = new uint32_t[FIND];
  auto* result_lookup = new uint32_t[FIND];
  memset(result_del, 0, sizeof(uint32_t) * FIND);
  memset(h_keys_lookup, 0, sizeof(uint32_t) * FIND);

  // Use shared files filled with random numbers
  path cwd = std::filesystem::current_path();
  path path_insert_keys = cwd / "random_keys_insert.bin";
  path path_insert_values = cwd / "random_values_insert.bin";
  path path_delete = cwd / "random_keys_delete.bin";
  path path_search = cwd / "random_keys_delete.bin";

  assert(std::filesystem::exists(path_insert_keys));
  assert(std::filesystem::exists(path_insert_values));
  assert(std::filesystem::exists(path_delete));
  assert(std::filesystem::exists(path_search));

  // Read data from file
  auto* tmp_keys_insert = new uint32_t[ADD];
  read_data(path_insert_keys, ADD, tmp_keys_insert);
  auto* tmp_values_insert = new uint32_t[ADD];
  read_data(path_insert_values, ADD, tmp_values_insert);
  for (int i = 0; i < ADD; i++) {
    h_kvs_insert[i].key = tmp_keys_insert[i];
    h_kvs_insert[i].value = tmp_values_insert[i];
  }
  delete[] tmp_keys_insert;
  delete[] tmp_values_insert;

  if (REM > 0) {
    auto* tmp_keys_delete = new uint32_t[REM];
    read_data(path_delete, REM, tmp_keys_delete);
    for (int i = 0; i < REM; i++) {
      h_keys_del[i] = tmp_keys_delete[i];
    }
    delete[] tmp_keys_delete;
  }

  if (FIND > 0) {
    auto* tmp_keys_search = new uint32_t[FIND];
    read_data(path_search, FIND, tmp_keys_search);
    for (int i = 0; i < FIND; i++) {
      h_keys_lookup[i] = tmp_keys_search[i];
    }
    delete[] tmp_keys_search;
  }

  size_t numThreads = 16;

  // Initialize hash table
    HashTable ht;
    if (!initHashTable(&ht, 16, 0.8)) {
        std::cerr << "Failed to initialize hash table!" << std::endl;
        return -1;
    }

    //Intilialize TBB hash table
    concurrent_hash_map<uint32_t, uint32_t> hashMap;

  float total_insert_time = 0.0F;
  float total_delete_time = 0.0F;
  float total_search_time = 0.0F;

  HRTimer start, end;
  uint32_t del_runs = 0, search_runs = 0;
  for (uint32_t i = 0; i < runs; i++) {
    start = HR::now();
    if(TBB){
      batch_insert_TBB(&hashMap, h_kvs_insert, result_insert, ADD, numThreads);
    }
    else 
      batch_insert(&ht, h_kvs_insert, result_insert, ADD, numThreads);
    end = HR::now();
    float iter_insert_time = duration_cast<milliseconds>(end - start).count();
    total_insert_time += iter_insert_time;

    if (REM > 0) {
      start = HR::now();
      if(TBB){
        batch_delete_TBB(&hashMap, h_keys_del, result_del, REM, numThreads);
      }
      else
        batch_delete(&ht, h_keys_del, result_del, REM, numThreads);
      end = HR::now();
      float iter_delete_time = duration_cast<milliseconds>(end - start).count();
      del_runs++;
      total_delete_time += iter_delete_time;
    }

    if (FIND > 0) {
      start = HR::now();
      if(TBB){
        batch_lookup_TBB(&hashMap, h_keys_lookup, result_lookup, FIND, numThreads);
      }
      else
        batch_lookup(&ht, h_keys_lookup, result_lookup, FIND, numThreads);
      end = HR::now();
      float iter_search_time = duration_cast<milliseconds>(end - start).count();
      search_runs++;
      total_search_time += iter_search_time;
    }
  }

  // print_table(&ht);
      for (uint32_t i = 0; i < hashMap.size(); ++i) {
        concurrent_hash_map<uint32_t, uint32_t>::const_accessor accessor;
        if (hashMap.find(accessor, i)) {
            std::cout << "Key: " << i << ", Updated Value: " << accessor->second << std::endl;
        }
    }

  cout << "Time taken by insert kernel (s): " << total_insert_time / (runs*1000)
       << "\n";
  if (del_runs > 0) {
    cout << "Time taken by delete kernel (s): " << total_delete_time / (del_runs*1000)
         << "\n";
  }
  if (search_runs > 0) {
    cout << "Time taken by search kernel (s): "
         << total_search_time / (search_runs*1000) << "\n";
  }

  return EXIT_SUCCESS;
}
// g++ -std=c++17 -fopenmp -pthread -o prob1.out hash_table.cpp  problem1.cpp -I/usr/local/tbb/include -L/usr/include/tbb -ltbb
//./prob1.out -ops=10000 -rns=2 -add=40 -rem=40