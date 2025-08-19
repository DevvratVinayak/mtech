#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <cstdint>
#include <pthread.h>
#include <iostream>

static const uint32_t SENTINEL_KEY = 0;
static const uint32_t SENTINEL_VALUE = 0;
static const uint32_t TOMBSTONE_KEY = UINT32_MAX;
static const float LOAD_FACTOR_THRESHOLD = 0.8;

typedef struct {
    uint64_t* table;             // Array of key-value pairs (packed)
    bool* isOccupied;            // Boolean array to mark occupied slots
    size_t size;                 // Current size of the hash table (number of elements)
    size_t capacity;             // Capacity of the hash table
    bool is_resizing;            // Whether a resize operation is in progress
    pthread_mutex_t lock;        // Common lock for all operations
} HashTable;

bool initHashTable(HashTable* ht, size_t capacity, float loadFactor);
bool insert(HashTable* ht, uint32_t key, uint32_t value);
bool deleteEntry(HashTable* ht, uint32_t key);
uint32_t lookup(HashTable* ht, uint32_t key);
void print_table(HashTable* ht);
void resize(HashTable* ht);

inline uint64_t packKeyValue(uint32_t key, uint32_t val);
inline void unpackKeyValue(uint64_t value, uint32_t& key, uint32_t& val);

#endif
