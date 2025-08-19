#include <cassert>
#include <chrono>
#include <iostream>
#include<iomanip>
#include<papi.h>

using namespace std;
using namespace std::chrono;

using HR = high_resolution_clock;
using HRTimer = HR::time_point;

#define N (2048)

void matmul_ijk(const uint32_t *A, const uint32_t *B, uint32_t *C, const int SIZE) {
  for (int i = 0; i < SIZE; i++) {
    for (int j = 0; j < SIZE; j++) {
      uint32_t sum = 0.0;
      for (int k = 0; k < SIZE; k++) {
        sum += A[i * SIZE + k] * B[k * SIZE + j];
      }
      C[i * SIZE + j] += sum;
    }
  }
}

/*  Block size of each array may be differnt*/
void matmul_ijk_blocking(const uint32_t *A, const uint32_t *B, uint32_t *C, const int SIZE, const int A_BLOCK_SIZE, const int B_BLOCK_SIZE, const int C_BLOCK_SIZE) {
  for (int i = 0; i < SIZE; i+= A_BLOCK_SIZE){
    for (int j = 0; j < SIZE; j+= B_BLOCK_SIZE){
      for (int k = 0; k < SIZE; k+= C_BLOCK_SIZE){
        /* Processing blocks */
        for (int i1 = i; i1 < min(i+A_BLOCK_SIZE,N); i1++){
          for (int j1 = j; j1 < min(j+B_BLOCK_SIZE,N); j1++){
            /* B×B mini -matrix (blocks) multiplications */
            int sum = 0;
            for (int k1 = k; k1 < min(k+C_BLOCK_SIZE,N); k1++){
              sum += A[i1*SIZE + k1]*B[k1*SIZE + j1];
            }
            C[i1*SIZE+j1]+=sum;
          }
        }
      }
    }
  }
}

/*  Block size of each array are same*/
void matmul_ijk_blocking(const uint32_t *A, const uint32_t *B, uint32_t *C, const int SIZE, const int BLOCK_SIZE) {
  for (int i = 0; i < SIZE; i+= BLOCK_SIZE){
    for (int j = 0; j < SIZE; j+= BLOCK_SIZE){
      for (int k = 0; k < SIZE; k+= BLOCK_SIZE){
        /* Processing blocks */
        for (int i1 = i; i1 < min(i+BLOCK_SIZE,N); i1++){
          for (int j1 = j; j1 < min(j+BLOCK_SIZE,N); j1++){
            /* B×B mini -matrix (blocks) multiplications */
            int sum = 0;
            for (int k1 = k; k1 < min(k+BLOCK_SIZE,N); k1++){
              sum += A[i1*SIZE + k1]*B[k1*SIZE + j1];
            }
            C[i1*SIZE+j1]+=sum;
          }
        }
      }
    }
  }
}


void matmul_ijk_blocking(const uint32_t *A, const uint32_t *B, uint32_t *C, const int SIZE) {
  //Best Case [64,32,16]
  matmul_ijk_blocking(A,B,C,SIZE,64,32,16);
  //Worst case [4,4,4]
  //matmul_ijk_blocking(A,B,C,SIZE,4);  
}

void init(uint32_t *mat, const int SIZE) {
  for (int i = 0; i < SIZE; i++) {
    for (int j = 0; j < SIZE; j++) {
      mat[i * SIZE + j] = 1;
    }
  }
}

void print_matrix(const uint32_t *mat, const int SIZE) {
  for (int i = 0; i < SIZE; i++) {
    for (int j = 0; j < SIZE; j++) {
      cout << mat[i * SIZE + j] << "\t";
    }
    cout << "\n";
  }
}

void check_result(const uint32_t *ref, const uint32_t *opt, const int SIZE) {
  for (int i = 0; i < SIZE; i++) {
    for (int j = 0; j < SIZE; j++) {
      if (ref[i * SIZE + j] != opt[i * SIZE + j]) {
        assert(false && "Diff found between sequential and blocked versions!\n");
      }
    }
  }
}
/*
  * Command used for compilation:
  * g++ -O3 -std=c++17 matmul-blocking.cpp -lpapi
*/

int main() {

  long_long counters[4];

  /* Initialize the PAPI library */
  int retval = PAPI_library_init(PAPI_VER_CURRENT);
  if (retval != PAPI_VER_CURRENT && retval > 0) {
    cerr << "PAPI library version mismatch: " << retval << " != " << PAPI_VER_CURRENT << "\n";
    exit(EXIT_FAILURE);
  } else if (retval < 0) {
    cerr << "PAPI library initialization error: " << retval << " != " << PAPI_VER_CURRENT << "\n";
    exit(EXIT_FAILURE);
  }

  int eventset = PAPI_NULL;
  retval = PAPI_create_eventset(&eventset);
  if (PAPI_OK != retval) {
    cerr << "Error at PAPI_create_eventset()" << endl;
    exit(EXIT_FAILURE);
  }

  if (PAPI_add_event(eventset, PAPI_TOT_INS) != PAPI_OK) {
    cout << "Error in PAPI_add_event PAPI_TOT_INS!\n";
    exit(EXIT_FAILURE);
  }
  if (PAPI_add_event(eventset, PAPI_TOT_CYC) != PAPI_OK) {
    cout << "Error in PAPI_add_event PAPI_TOT_CYC!\n";
    exit(EXIT_FAILURE);
  }

  if (PAPI_add_event(eventset, PAPI_L1_TCM) != PAPI_OK) {
    cout << "Error in PAPI_add_event PAPI_L1_TCM!\n";
    exit(EXIT_FAILURE);
  }

if (PAPI_add_event(eventset, PAPI_L2_TCM) != PAPI_OK) {
    cout << "Error in PAPI_add_event PAPI_L2_TCM!\n";
    exit(EXIT_FAILURE);
  } 
  retval = PAPI_start(eventset);
  if (PAPI_OK != retval) {
    cerr << "Error at PAPI_start()" << endl;
    exit(EXIT_FAILURE);
  }

  uint32_t *A = new uint32_t[N * N];
  uint32_t *B = new uint32_t[N * N];
  uint32_t *C_seq = new uint32_t[N * N];

  init(A, N);
  init(B, N);
  init(C_seq, N);

  HRTimer start = HR::now();
  matmul_ijk(A, B, C_seq, N);
  HRTimer end = HR::now();
  auto duration1 = duration_cast<microseconds>(end - start).count();
  cout << "Time without blocking (us): " << duration1 << "\n";

  uint32_t *C_blk = new uint32_t[N * N];
  init(C_blk, N);

  start = HR::now();
  matmul_ijk_blocking(A, B, C_blk, N);
  end = HR::now();
  auto duration2 = duration_cast<microseconds>(end - start).count();

  if (PAPI_read(eventset, counters) != PAPI_OK){
    cerr << "PAPI library initialization error: " << retval << " != " << PAPI_VER_CURRENT << "\n";
    exit(EXIT_FAILURE);
  }
  
  
  printf("Instructions Completed : %lld \n", counters[0]);
  printf("Total cycles : %lld \n", counters[1]);
  printf("Level 1 total cache accesses : %lld \n", counters[2]);
  printf("Level 2 total cache accesses : %lld \n", counters[3]);
  
  
  if (PAPI_stop(eventset, counters) != PAPI_OK){
    cerr << "PAPI library initialization error: " << retval << " != " << PAPI_VER_CURRENT << "\n";
    exit(EXIT_FAILURE);
  }
  cout << "Time with blocking (us): " << duration2 << "\n";
  if(duration1 < duration2){
    cout << "No speed up acheived!";
  }
  else{
    double speedup = ((double)duration1/(double)duration2);
    cout << "Speed Up achieved " << speedup <<endl ;
  }
  check_result(C_seq, C_blk, N);
  return EXIT_SUCCESS;
}
