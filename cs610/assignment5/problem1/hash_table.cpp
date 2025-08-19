#include "hash_table.h"
#include <iostream>
#include <pthread.h>
#include <cstdlib>
#include <cmath> 

size_t primaryHash(uint32_t key, size_t tableSize) {
    return key % tableSize;
}

size_t secondaryHash(uint32_t key, size_t tableSize) {
    return 1 + (key % (tableSize - 1));  // ensures step size is non-zero
}

uint32_t multiplicativeHash(uint32_t key, uint32_t tableSize) {
    const double A = 0.6180339887;  // Constant for multiplicative hashing
    return static_cast<uint32_t>(tableSize * (key * A - std::floor(key * A)));
}
uint32_t fnv1Hash(uint32_t key, uint32_t tableSize) {
    const uint32_t FNV_offset_basis = 2166136261U;
    const uint32_t FNV_prime = 16777619U;

    uint32_t hash = FNV_offset_basis;
    hash ^= key;
    hash *= FNV_prime;

    return hash % tableSize;  
}

uint32_t murmurHash(uint32_t key, uint32_t tableSize) {
    uint32_t seed = 0x1234ABCD;
    uint32_t hash = seed;

    hash ^= key;
    hash *= 0x5bd1e995;
    hash ^= hash >> 15;

    return hash % tableSize;  
}

// Initialize the hash table
bool initHashTable(HashTable* ht, size_t capacity, float loadFactor) {
    ht->capacity = capacity;
    ht->size = 0;
    ht->is_resizing = false;
    ht->table = new uint64_t[capacity];
    ht->isOccupied = new bool[capacity];

    for (size_t i = 0; i < capacity; ++i) {
        ht->table[i] = 0;          // Initially, the table is empty (0 value)
        ht->isOccupied[i] = false; // Marks the slot as unoccupied
    }

    pthread_mutex_init(&ht->lock, nullptr);
    return true;
}

// Pack key-value into a 64-bit integer
inline uint64_t packKeyValue(uint32_t key, uint32_t val) {
    return (static_cast<uint64_t>(key) << 32) | (static_cast<uint32_t>(val) & 0xFFFFFFFF);
}

// Function to unpack a 64-bit integer into two 32-bit integers
inline void unpackKeyValue(uint64_t value, uint32_t& key, uint32_t& val) {
    key = static_cast<uint32_t>(value >> 32);
    val = static_cast<uint32_t>(value & 0xFFFFFFFF);
}

// Insert a key-value pair into the table using double hashing for collision resolution
bool insert(HashTable* ht, uint32_t key, uint32_t value) {
    pthread_mutex_lock(&ht->lock);  // Lock the common lock for all operations

    // Trigger resize if the load factor exceeds the threshold
    if ((float)ht->size / ht->capacity > LOAD_FACTOR_THRESHOLD) {
        if (!ht->is_resizing) {
            ht->is_resizing = true;  // Set the resizing flag to true
            resize(ht);
        }
    }

    size_t index = murmurHash(key, ht->capacity);  // Primary hash : can be replaced by other defined hash functions
    size_t step = secondaryHash(key, ht->capacity); // Secondary hash

    size_t retries = 0;
    while (retries < ht->capacity) {
        // If the slot is empty or a tombstone is present, insert the key-value pair
        if (!ht->isOccupied[index] || ht->table[index] == 0 || ht->table[index] == TOMBSTONE_KEY) {
            ht->table[index] = packKeyValue(key, value);
            ht->isOccupied[index] = true;
            ht->size++;
            pthread_mutex_unlock(&ht->lock);  // Unlock after inserting
            return true;
        }

        index = (index + step) % ht->capacity;
        retries++;
    }

    pthread_mutex_unlock(&ht->lock);  // Unlock in case of failure
    return false;  
}

// Delete an entry from the hash table by key
bool deleteEntry(HashTable* ht, uint32_t key) {
    pthread_mutex_lock(&ht->lock);  

    size_t index = murmurHash(key, ht->capacity);
    size_t step = secondaryHash(key, ht->capacity);

    size_t retries = 0;
    while (retries < ht->capacity) {
        if (!ht->isOccupied[index]) {
            pthread_mutex_unlock(&ht->lock);  // Unlock if key is not found
            return false;  // Key not found
        }

        uint32_t currentKey, currentVal;
        unpackKeyValue(ht->table[index], currentKey, currentVal);

        if (currentKey == key) {
            ht->table[index] = TOMBSTONE_KEY;  // Mark as deleted
            ht->isOccupied[index] = true;
            ht->size--;
            pthread_mutex_unlock(&ht->lock);  // Unlock after deletion
            return true;
        }

        // If the key doesn't match, check the next slot
        index = (index + step) % ht->capacity;
        retries++;
    }

    pthread_mutex_unlock(&ht->lock);  
    return false;  
}

// Lookup a key in the hash table
uint32_t lookup(HashTable* ht, uint32_t key) {
    pthread_mutex_lock(&ht->lock);  

    size_t index = murmurHash(key, ht->capacity);
    size_t step = secondaryHash(key, ht->capacity);

    size_t retries = 0;
    while (retries < ht->capacity) {
        uint32_t currentKey, currentVal;
        unpackKeyValue(ht->table[index], currentKey, currentVal);
        if (currentKey == key) {
            // printf("index : %ld, val : %d\n", index, currentVal);
            pthread_mutex_unlock(&ht->lock);  
            return currentVal;  
        }
        // If the key doesn't match, check the next slot
        index = (index + step) % ht->capacity;
        retries++;
    }
    pthread_mutex_unlock(&ht->lock); 
    return SENTINEL_VALUE;  
}

// Print the contents of the hash table
void print_table(HashTable* ht) {
    pthread_mutex_lock(&ht->lock);  
    std::cout << "HashTable Contents (Capacity: " << ht->capacity << ", Size: " << ht->size << "):\n";
    for (size_t i = 0; i < ht->capacity; ++i) {
        if (ht->isOccupied[i] && ht->table[i] != 0 && ht->table[i] != TOMBSTONE_KEY) {
            uint32_t key, value;
            unpackKeyValue(ht->table[i], key, value);
            std::cout << "Index " << i << ": Key = " << key << ", Value = " << value << "\n";
        } else if (ht->isOccupied[i] && ht->table[i] == TOMBSTONE_KEY) {
            std::cout << "Index " << i << ": Tombstone (deleted entry)\n";
        } else {
            std::cout << "Index " << i << ": Empty\n";
        }
    }
    std::cout << std::endl;

    pthread_mutex_unlock(&ht->lock);  
}

// Resize the hash table (doubling the capacity)
void resize(HashTable* ht) {
    size_t newCapacity = ht->capacity * 2;
    uint64_t* newTable = new uint64_t[newCapacity];
    bool* newOccupied = new bool[newCapacity];

    // Initialize the new table with empty values
    for (size_t i = 0; i < newCapacity; ++i) {
        newTable[i] = 0;
        newOccupied[i] = false;
    }

    // Rehash all existing elements into the new table
    for (size_t i = 0; i < ht->capacity; ++i) {
        if (ht->isOccupied[i] && ht->table[i] != 0 && ht->table[i] != TOMBSTONE_KEY) {
            uint32_t key, value;
            unpackKeyValue(ht->table[i], key, value);

            size_t index = murmurHash(key, newCapacity);
            size_t step = secondaryHash(key, newCapacity);

            // Find an available slot using double hashing
            while (newOccupied[index]) {
                index = (index + step) % newCapacity;
            }

            // Insert into the new table
            newTable[index] = packKeyValue(key, value);
            newOccupied[index] = true;
        }
    }

    delete[] ht->table;
    delete[] ht->isOccupied;

    // Update the table and capacity
    ht->table = newTable;
    ht->isOccupied = newOccupied;
    ht->capacity = newCapacity;

    ht->is_resizing = false;  // Mark resizing as complete
}
