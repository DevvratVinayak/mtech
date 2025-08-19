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
#include <omp.h>

using std::cout;
using std::endl;
using std::string;

typedef struct {
  uint32_t key;
  uint32_t value;
} KeyValue;

void test_batch_insert(HashTable* ht, KeyValue* kv_pairs, bool* result, size_t batchSize) {
    for (size_t i = 0; i < batchSize; ++i) {
        result[i] = insert(ht, kv_pairs[i].key, kv_pairs[i].value);  // Insert key and value separately
    }
    bool success = true;
    for(int i=0; i<batchSize; i++){
        if(!result[i]){
            success = false;
            break;
        }
    }
    if(!success)  {
        cout << "Insert does not work as expected." << endl;
    }
    else {
        cout << "Insert works as expected." << endl;
    }
}

void test_batch_delete(HashTable* ht, uint32_t* keys, bool* result, size_t batchSize) {
    for (size_t i = 0; i < batchSize; ++i) {
        // printf("%d::::::::::::::::\n", lookup(ht, 20));
        result[i] = deleteEntry(ht, keys[i]);  
    }
    bool success = true;
    for(int i=0; i<batchSize; i++){
        if(!result[i]){
            success = false;
            break;
        }
    }
    if(!success)  {
        cout << "Delete does not work as expected." << endl;
    }
    else {
        cout << "Delete works as expected." << endl;
    }
}


void test_batch_lookup(HashTable* ht, uint32_t* keys, uint32_t* result, size_t batchSize) {
    for (size_t i = 0; i < batchSize; ++i) {
        // printf("%d::::::::::::::::\n", lookup(ht, 7723));
        result[i] = lookup(ht, keys[i]); 
    }
    if(result[0] == SENTINEL_VALUE && result[1] == SENTINEL_VALUE && result[2] == 110 && result[3] == 120 && result[4] == 130  )
    {
        cout << "Lookup works as expected." << endl;
    }
    else {
        cout << "Lookup does not as expected." << endl;
    }
}

int main(int argc, char* argv[]) {
  
  uint64_t ADD = 20;
  uint64_t REM = 10;
  uint64_t FIND = 5;

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

  for (int i = 0; i < ADD; i++) {
    h_kvs_insert[i].key = i+1;
    h_kvs_insert[i].value = (i+1)*10;
  }
  for (int i = 0; i < REM; i++) {
      h_keys_del[i] = i+1;
   }

   h_keys_lookup[0] = 9;
   h_keys_lookup[1] = 10;
   h_keys_lookup[2] = 11;
   h_keys_lookup[3] = 12;
   h_keys_lookup[4] = 13;

  // Initialize hash table
    HashTable ht;
    if (!initHashTable(&ht, 16, 0.8)) {
        std::cerr << "Failed to initialize hash table!" << std::endl;
        return -1;
    };
    test_batch_insert(&ht, h_kvs_insert, result_insert, ADD);
    // cout << "Hash Table after insertions : " << endl;
    // print_table(&ht);
    test_batch_delete(&ht, h_keys_del, result_del, REM);
    // cout << "Hash Table after deletions : " << endl;
    // print_table(&ht);
    test_batch_lookup(&ht, h_keys_lookup, result_lookup, FIND);

    return EXIT_SUCCESS;
}
