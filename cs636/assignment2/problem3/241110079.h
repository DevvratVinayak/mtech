#include <iostream>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <math.h>
#include <atomic>
#include <sstream>
#include <iomanip>
#define BLOOM_SIZE (1 << 24) 
#define BIT_ARRAY_SIZE (BLOOM_SIZE / 8 + (BLOOM_SIZE % 8 != 0))
#define STRIPE_COUNT 64 
#define STRIPE_SIZE (BIT_ARRAY_SIZE / STRIPE_COUNT + (BIT_ARRAY_SIZE % STRIPE_COUNT != 0))
using namespace std;
typedef struct {
    uint8_t *bit_array;
    pthread_mutex_t locks[STRIPE_COUNT];
} BloomFilter;

uint32_t hash1(uint32_t x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x % BLOOM_SIZE;
}

uint32_t hash2(uint32_t x) {
    x ^= x >> 17;
    x *= 0xed5ad4bb;
    x ^= x >> 11;
    x *= 0xac4c1b51;
    x ^= x >> 15;
    x *= 0x31848bab;
    x ^= x >> 14;
    return x % BLOOM_SIZE;
}

uint32_t hash3(uint32_t x) {
    x = (x ^ 61) ^ (x >> 16);
    x = x + (x << 3);
    x = x ^ (x >> 4);
    x = x * 0x27d4eb2d;
    x = x ^ (x >> 15);
    return x % BLOOM_SIZE;
}

void bloom_init(BloomFilter *filter) {
    filter->bit_array =  (uint8_t*)calloc(BIT_ARRAY_SIZE, sizeof(uint8_t));
    for (int i = 0; i < STRIPE_COUNT; i++) {
        pthread_mutex_init(&filter->locks[i], NULL);
    }
}

void bloom_destroy(BloomFilter *filter) {
    free(filter->bit_array);
    for (int i = 0; i < STRIPE_COUNT; i++) {
        pthread_mutex_destroy(&filter->locks[i]);
    }
}

// Get stripe index for a bit position
inline int get_stripe(uint32_t bit_pos) {
    return (bit_pos / 8) / STRIPE_SIZE;
}

void add(BloomFilter *filter, uint32_t value) {
    uint32_t h1 = hash1(value);
    uint32_t h2 = hash2(value);
    uint32_t h3 = hash3(value);
    
    int stripe1 = get_stripe(h1);
    int stripe2 = get_stripe(h2);
    int stripe3 = get_stripe(h3);

    // Lock stripes in order to prevent deadlocks
    if (stripe1 == stripe2 && stripe2 == stripe3) {
        // All bits in same stripe - single lock
        pthread_mutex_lock(&filter->locks[stripe1]);
        filter->bit_array[h1 / 8] |= (1 << (h1 % 8));
        filter->bit_array[h2 / 8] |= (1 << (h2 % 8));
        filter->bit_array[h3 / 8] |= (1 << (h3 % 8));
        pthread_mutex_unlock(&filter->locks[stripe1]);
    } 
    else if (stripe1 == stripe2) {
        // Two stripes to lock
        pthread_mutex_lock(&filter->locks[stripe1]);
        pthread_mutex_lock(&filter->locks[stripe3]);
        filter->bit_array[h1 / 8] |= (1 << (h1 % 8));
        filter->bit_array[h2 / 8] |= (1 << (h2 % 8));
        filter->bit_array[h3 / 8] |= (1 << (h3 % 8));
        pthread_mutex_unlock(&filter->locks[stripe3]);
        pthread_mutex_unlock(&filter->locks[stripe1]);
    }
    else if (stripe1 == stripe3) {
        pthread_mutex_lock(&filter->locks[stripe1]);
        pthread_mutex_lock(&filter->locks[stripe2]);
        filter->bit_array[h1 / 8] |= (1 << (h1 % 8));
        filter->bit_array[h2 / 8] |= (1 << (h2 % 8));
        filter->bit_array[h3 / 8] |= (1 << (h3 % 8));
        pthread_mutex_unlock(&filter->locks[stripe2]);
        pthread_mutex_unlock(&filter->locks[stripe1]);
    }
    else if (stripe2 == stripe3) {
        pthread_mutex_lock(&filter->locks[stripe2]);
        pthread_mutex_lock(&filter->locks[stripe1]);
        filter->bit_array[h1 / 8] |= (1 << (h1 % 8));
        filter->bit_array[h2 / 8] |= (1 << (h2 % 8));
        filter->bit_array[h3 / 8] |= (1 << (h3 % 8));
        pthread_mutex_unlock(&filter->locks[stripe1]);
        pthread_mutex_unlock(&filter->locks[stripe2]);
    }
    else {
        // All three stripes different - lock in order
        if (stripe1 < stripe2 && stripe1 < stripe3) {
            pthread_mutex_lock(&filter->locks[stripe1]);
            if (stripe2 < stripe3) {
                pthread_mutex_lock(&filter->locks[stripe2]);
                pthread_mutex_lock(&filter->locks[stripe3]);
            } else {
                pthread_mutex_lock(&filter->locks[stripe3]);
                pthread_mutex_lock(&filter->locks[stripe2]);
            }
        }
        else if (stripe2 < stripe1 && stripe2 < stripe3) {
            pthread_mutex_lock(&filter->locks[stripe2]);
            if (stripe1 < stripe3) {
                pthread_mutex_lock(&filter->locks[stripe1]);
                pthread_mutex_lock(&filter->locks[stripe3]);
            } else {
                pthread_mutex_lock(&filter->locks[stripe3]);
                pthread_mutex_lock(&filter->locks[stripe1]);
            }
        }
        else {
            pthread_mutex_lock(&filter->locks[stripe3]);
            if (stripe1 < stripe2) {
                pthread_mutex_lock(&filter->locks[stripe1]);
                pthread_mutex_lock(&filter->locks[stripe2]);
            } else {
                pthread_mutex_lock(&filter->locks[stripe2]);
                pthread_mutex_lock(&filter->locks[stripe1]);
            }
        }
        
        filter->bit_array[h1 / 8] |= (1 << (h1 % 8));
        filter->bit_array[h2 / 8] |= (1 << (h2 % 8));
        filter->bit_array[h3 / 8] |= (1 << (h3 % 8));
        // Unlock in reverse order
        pthread_mutex_unlock(&filter->locks[stripe3]);
        pthread_mutex_unlock(&filter->locks[stripe2]);
        pthread_mutex_unlock(&filter->locks[stripe1]);
    }
}

bool contains(BloomFilter *filter, uint32_t value) {
    uint32_t h1 = hash1(value);
    uint32_t h2 = hash2(value);
    uint32_t h3 = hash3(value);
    
    int stripe1 = get_stripe(h1);
    int stripe2 = get_stripe(h2);
    int stripe3 = get_stripe(h3);
    
    bool b1, b2, b3;
    
    // Lock stripes in order to prevent deadlocks
    if (stripe1 == stripe2 && stripe2 == stripe3) {
        pthread_mutex_lock(&filter->locks[stripe1]);
        b1 = filter->bit_array[h1 / 8] & (1 << (h1 % 8));
        b2 = filter->bit_array[h2 / 8] & (1 << (h2 % 8));
        b3 = filter->bit_array[h3 / 8] & (1 << (h3 % 8));
        pthread_mutex_unlock(&filter->locks[stripe1]);
    }
    else {
        // Need to lock all relevant stripes
        pthread_mutex_lock(&filter->locks[stripe1]);
        b1 = filter->bit_array[h1 / 8] & (1 << (h1 % 8));
        pthread_mutex_unlock(&filter->locks[stripe1]);
        
        pthread_mutex_lock(&filter->locks[stripe2]);
        b2 = filter->bit_array[h2 / 8] & (1 << (h2 % 8));
        pthread_mutex_unlock(&filter->locks[stripe2]);
        
        pthread_mutex_lock(&filter->locks[stripe3]);
        b3 = filter->bit_array[h3 / 8] & (1 << (h3 % 8));
        pthread_mutex_unlock(&filter->locks[stripe3]);
    }
    
    return b1 && b2 && b3;
}

// Print the first N bytes of the filter with proper locking
void print(BloomFilter *filter) {
    printf("Bloom Filter Contents:\n");
    for (uint32_t i = 0; i < BIT_ARRAY_SIZE; ++i) {
        uint8_t byte = filter->bit_array[i];
        for (int bit = 0; bit < 8; ++bit) {
            printf("%d", (byte >> bit) & 1);
        }
        if ((i + 1) % 8 == 0) {
            printf(" "); 
        }
    }
    printf("\n");
}

// Print statistics with proper math support
std::string print_stats(BloomFilter *filter) {
    size_t total_bits = BIT_ARRAY_SIZE * 8;
    size_t set_bits = 0;

    // Lock all stripes for consistent count
    for (int i = 0; i < STRIPE_COUNT; i++) {
        pthread_mutex_lock(&filter->locks[i]);
    }

    for (size_t i = 0; i < BIT_ARRAY_SIZE; i++) {
        uint8_t byte = filter->bit_array[i];
        for (int j = 0; j < 8; j++) {
            if ((byte >> j) & 1) set_bits++;
        }
    }

    // Unlock all stripes
    for (int i = 0; i < STRIPE_COUNT; i++) {
        pthread_mutex_unlock(&filter->locks[i]);
    }
    std::ostringstream oss;
    double percent_set = (100.0 * set_bits) / total_bits;


    oss << "Bloom Filter Statistics:\n";
    oss << "  Size: " << total_bits << " bits (" << BIT_ARRAY_SIZE << " bytes)\n";
    oss << "  Set bits: " << set_bits << " (" << std::fixed << std::setprecision(2) << percent_set << "%)\n";

    if (set_bits > 0 && set_bits < total_bits) {
        double ratio = static_cast<double>(set_bits) / total_bits;
        double estimated_elements = -(static_cast<double>(total_bits) / 3.0) * std::log(1.0 - ratio);
        oss << "  Estimated elements: ~" << std::round(estimated_elements) << "\n";
    } else if (set_bits == total_bits) {
        oss << "  Estimated elements: Filter saturated (too many elements)\n";
    } else {
        oss << "  Estimated elements: 0\n";
    }

    return oss.str();
}