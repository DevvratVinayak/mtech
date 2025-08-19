#include <pthread.h>
#include <vector>
#include <atomic>
#include <functional>
#include <omp.h>
#include <cmath>
#include <sstream>

// Initial bucket count (will grow as needed)
static constexpr uint32_t INITIAL_BUCKET_COUNT = 1000;
static constexpr uint64_t MAX_OPERATIONS = 1e+15;
int NUM_THREADS=1; /*Customize the number of threads*/

// Resizing thresholds
static constexpr uint32_t BUCKET_THRESHOLD = 100;  // Max items per bucket before considering resize
static constexpr float OVERLOAD_RATIO = 0.25f;    // Ratio of overloaded buckets that triggers resize
static constexpr uint32_t GLOBAL_THRESHOLD = 20;  // Max items in any single bucket that forces resize

/*          Definitions            */

typedef struct {
    uint32_t key;
    uint32_t value;
} KeyValue;

typedef struct Node {
    uint64_t packed_kv; // Using packed key-value to save space
    struct Node* next;
} Node;

typedef struct {
    Node* head;
    pthread_mutex_t lock;
    std::atomic<uint32_t> count; // Track number of items in this bucket
} Bucket;

// Thread-local storage for memory pool
__thread Node* thread_local_pool = nullptr;

typedef struct {
    Bucket* buckets;
    uint32_t bucket_count;
    std::atomic<Node*> node_pool;
    std::function<uint32_t(uint32_t, uint32_t)> primary_hash;
    std::function<uint32_t(uint32_t, uint32_t)> secondary_hash;
    std::atomic<uint32_t> overloaded_buckets; // Count of buckets exceeding BUCKET_THRESHOLD
} HashTable;

/*          Utility Functions            */

// Pack key-value into a 64-bit integer
inline uint64_t packKeyValue(uint32_t key, uint32_t val) {
    return (static_cast<uint64_t>(key) << 32) |
           (static_cast<uint32_t>(val) & 0xFFFFFFFF);
}

// Function to unpack a 64-bit integer into two 32-bit integers
inline void unpackKeyValue(uint64_t value, uint32_t& key, uint32_t& val) {
    key = static_cast<uint32_t>(value >> 32);
    val = static_cast<uint32_t>(value & 0xFFFFFFFF);
}

// Initialize hash functions with current bucket count
void init_hash_functions(HashTable* ht) {

    ht->primary_hash = [](uint32_t key, uint32_t bucket_count){
        const double A = 0.6180339887;  // Constant for multiplicative hashing
        return static_cast<uint32_t>(bucket_count * (key * A - std::floor(key * A)));
    };
    // Secondary hash function (for double hashing)
    ht->secondary_hash = [](uint32_t key, uint32_t bucket_count) {
        return 1 + (key % (bucket_count - 1)); // Ensures non-zero and coprime with size
    };
}

// Initialize hash table
void init_hash_table(HashTable* ht) {
    ht->bucket_count = INITIAL_BUCKET_COUNT;
    ht->buckets = new Bucket[ht->bucket_count];
    ht->overloaded_buckets.store(0);
    
    init_hash_functions(ht);
    for (uint32_t i = 0; i < ht->bucket_count; i++) {
        ht->buckets[i].head = nullptr;
        ht->buckets[i].count.store(0);
        pthread_mutex_init(&ht->buckets[i].lock, nullptr);
    }
    ht->node_pool.store(nullptr);
}

// Check if we need to resize and perform resize if needed
bool check_and_resize(HashTable* ht) {
    uint32_t overloaded = ht->overloaded_buckets.load();
    uint32_t current_size = ht->bucket_count;
    
    if ((overloaded > OVERLOAD_RATIO * current_size) || 
        (overloaded > 0 && GLOBAL_THRESHOLD > 0)) {
        uint32_t new_size = current_size * 2;
        // printf("Resizinf:::::::::::::::::::::\n");
        
        // Create new buckets array
        Bucket* new_buckets = new Bucket[new_size];
        for (uint32_t i = 0; i < new_size; i++) {
            new_buckets[i].head = nullptr;
            new_buckets[i].count.store(0);
            pthread_mutex_init(&new_buckets[i].lock, nullptr);
        }
        
        // Rehash all existing elements
        for (uint32_t i = 0; i < current_size; i++) {
            pthread_mutex_lock(&ht->buckets[i].lock);
            Node* current = ht->buckets[i].head;
            while (current) {
                uint32_t key, value;
                unpackKeyValue(current->packed_kv, key, value);
                
                // Find new bucket using new size
                uint32_t new_bucket_idx = ht->primary_hash(key, new_size);
                uint32_t secondary_offset = ht->secondary_hash(key, new_size);
                
                // Handle collisions with double hashing
                uint32_t attempts = 0;
                while (attempts < new_size) {
                    uint32_t bucket_idx = (new_bucket_idx + attempts * secondary_offset) % new_size;
                    
                    pthread_mutex_lock(&new_buckets[bucket_idx].lock);
                    if (new_buckets[bucket_idx].count.load() < BUCKET_THRESHOLD) {
                        // Insert into this bucket
                        Node* next = current->next;
                        current->next = new_buckets[bucket_idx].head;
                        new_buckets[bucket_idx].head = current;
                        new_buckets[bucket_idx].count.fetch_add(1);
                        pthread_mutex_unlock(&new_buckets[bucket_idx].lock);
                        break;
                    }
                    pthread_mutex_unlock(&new_buckets[bucket_idx].lock);
                    attempts++;
                }
                
                current = current->next;
            }
            pthread_mutex_unlock(&ht->buckets[i].lock);
        }
        
        // Swap in the new buckets array
        Bucket* old_buckets = ht->buckets;
        ht->buckets = new_buckets;
        ht->bucket_count = new_size;
        ht->overloaded_buckets.store(0);
        
        // Delete the old buckets array (but not the nodes)
        delete[] old_buckets;
        
        return true;
    }
    return false;
}

// Get node from pool or allocate new
Node* acquire_node(HashTable* ht) {
    if (thread_local_pool) {
        Node* n = thread_local_pool;
        thread_local_pool = thread_local_pool->next;
        return n;
    }

    Node* global_node = ht->node_pool.exchange(nullptr);
    if (global_node) {
        thread_local_pool = global_node->next;
        return global_node;
    }

    return new Node();
}

// Return node to pool
void release_node(HashTable* ht, Node* n) {
    n->next = thread_local_pool;
    thread_local_pool = n;
}

// Double hashing probe
uint32_t find_bucket(HashTable* ht, uint32_t key, bool for_insert) {
    uint32_t primary_idx = ht->primary_hash(key, ht->bucket_count);
    uint32_t secondary_offset = ht->secondary_hash(key, ht->bucket_count);
    uint32_t attempts = 0;
    
    while (attempts < ht->bucket_count) {
        uint32_t bucket_idx = (primary_idx + attempts * secondary_offset) % ht->bucket_count;
        
        // For insert, we can use any bucket
        if (for_insert) return bucket_idx;
        
        // For lookup/delete, we need to check if key exists in this bucket
        pthread_mutex_lock(&ht->buckets[bucket_idx].lock);
        Node* current = ht->buckets[bucket_idx].head;
        bool found = false;
        
        while (current) {
            uint32_t current_key, unused_val;
            unpackKeyValue(current->packed_kv, current_key, unused_val);
            if (current_key == key) {
                found = true;
                break;
            }
            current = current->next;
        }
        
        pthread_mutex_unlock(&ht->buckets[bucket_idx].lock);
        
        if (found) return bucket_idx;
        
        attempts++;
    }
    
    return primary_idx; // Fallback if all probes fail
}

// Batch operations with double hashing
void batch_insert(HashTable* ht, const KeyValue* kv_pairs, bool* results, size_t count) {
    std::vector<Node*> nodes(count);
    // Set number of threads for OpenMP
    omp_set_num_threads(NUM_THREADS);
    // Parallelize the insertion of key-value pairs using OpenMP
    #pragma omp parallel for
    for (size_t i = 0; i < count; i++) {
        nodes[i] = acquire_node(ht);
        nodes[i]->packed_kv = packKeyValue(kv_pairs[i].key, kv_pairs[i].value);
        nodes[i]->next = nullptr;
    }

    for (size_t i = 0; i < count; i++) {
        uint32_t key = kv_pairs[i].key;
        uint32_t bucket_idx = find_bucket(ht, key, true);
        Bucket* bucket = &ht->buckets[bucket_idx];
        bool success = false;

        pthread_mutex_lock(&bucket->lock);
        
        // Check for duplicates in this bucket
        Node* current = bucket->head;
        while (current) {
            uint32_t current_key, unused_val;
            unpackKeyValue(current->packed_kv, current_key, unused_val);
            if (current_key == key) break;
            current = current->next;
        }

        if (!current) {
            uint32_t prev_count = bucket->count.fetch_add(1);
            
            // Check if this bucket just crossed the threshold
            if (prev_count == BUCKET_THRESHOLD) {
                ht->overloaded_buckets.fetch_add(1);
            }
            
            nodes[i]->next = bucket->head;
            bucket->head = nodes[i];
            success = true;
            nodes[i] = nullptr;
        }

        pthread_mutex_unlock(&bucket->lock);
        results[i] = success;
        
        // Check if we need to resize after each insertion
        if (success) {
            check_and_resize(ht);
        }
    }

    for (size_t i = 0; i < count; i++) {
        if (nodes[i]) release_node(ht, nodes[i]);
    }
}

void batch_delete(HashTable* ht, const uint32_t* keys, bool* results, size_t count) {
    // Set number of threads for OpenMP
    omp_set_num_threads(NUM_THREADS);
    // Parallelize the insertion of key-value pairs using OpenMP
    #pragma omp parallel for
    for (size_t i = 0; i < count; i++) {
        uint32_t key = keys[i];
        uint32_t bucket_idx = find_bucket(ht, key, false);
        Bucket* bucket = &ht->buckets[bucket_idx];
        bool success = false;

        pthread_mutex_lock(&bucket->lock);
        
        Node** ptr = &bucket->head;
        while (*ptr) {
            uint32_t current_key, unused_val;
            unpackKeyValue((*ptr)->packed_kv, current_key, unused_val);
            
            if (current_key == key) {
                Node* to_delete = *ptr;
                *ptr = to_delete->next;
                
                uint32_t prev_count = bucket->count.fetch_sub(1);
                
                // Check if this bucket just dropped below threshold
                if (prev_count == BUCKET_THRESHOLD + 1) {
                    ht->overloaded_buckets.fetch_sub(1);
                }
                
                release_node(ht, to_delete);
                success = true;
                break;
            }
            ptr = &(*ptr)->next;
        }

        pthread_mutex_unlock(&bucket->lock);
        results[i] = success;
    }
}

void batch_lookup(HashTable* ht, const uint32_t* keys, uint32_t* results, size_t count) {
    // Set number of threads for OpenMP
    omp_set_num_threads(NUM_THREADS);
    // Parallelize the insertion of key-value pairs using OpenMP
    #pragma omp parallel for
    for (size_t i = 0; i < count; i++) {
        uint32_t key = keys[i];
        uint32_t bucket_idx = find_bucket(ht, key, false);
        Bucket* bucket = &ht->buckets[bucket_idx];
        uint32_t value = -1;

        pthread_mutex_lock(&bucket->lock);
        
        Node* current = bucket->head;
        while (current) {
            uint32_t current_key, current_val;
            unpackKeyValue(current->packed_kv, current_key, current_val);
            
            if (current_key == key) {
                value = current_val;
                break;
            }
            current = current->next;
        }

        pthread_mutex_unlock(&bucket->lock);
        results[i] = value;
    }
}

void destroy_hash_table(HashTable* ht) {
    for (uint32_t i = 0; i < ht->bucket_count; i++) {
        pthread_mutex_lock(&ht->buckets[i].lock);
        Node* current = ht->buckets[i].head;
        while (current) {
            Node* next = current->next;
            release_node(ht, current);
            current = next;
        }
        ht->buckets[i].head = nullptr;
        pthread_mutex_unlock(&ht->buckets[i].lock);
        pthread_mutex_destroy(&ht->buckets[i].lock);
    }

    delete[] ht->buckets;

    Node* pool_node = ht->node_pool.exchange(nullptr);
    while (pool_node) {
        Node* next = pool_node->next;
        delete pool_node;
        pool_node = next;
    }
}

std::string toString(HashTable* ht) {
    std::ostringstream oss;
    oss << "Hash Table Contents:\n";
    oss << "--------------------\n";
    oss << "Bucket Count: " << ht->bucket_count << "\n";
    oss << "Overloaded Buckets: " << ht->overloaded_buckets.load() << "\n";
    oss << "--------------------\n";
    for (uint32_t i = 0; i < ht->bucket_count; i++) {
        pthread_mutex_lock(&ht->buckets[i].lock);
        
        Node* current = ht->buckets[i].head;
        if (current) {
            oss << "Bucket " << i << " (Count: " << ht->buckets[i].count.load() << "):\n";
            while (current) {
                uint32_t key, value;
                unpackKeyValue(current->packed_kv, key, value);
                oss << "  [Key: " << key << ", Value: " << value << "]\n";
                current = current->next;
            }
        }
        
        pthread_mutex_unlock(&ht->buckets[i].lock);
    }
    oss << "--------------------\n";
    return oss.str();
}