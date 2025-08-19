#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <malloc.h>
#include <smmintrin.h>
#include <immintrin.h>
#include <algorithm>
using std::cout;
using std::endl;
using std::chrono::duration_cast;
using HR = std::chrono::high_resolution_clock;
using HRTimer = HR::time_point;
using std::chrono::microseconds;
using std::chrono::milliseconds;

const static float EPSILON = std::numeric_limits<float>::epsilon();

#define N (1024)
#define BLOCK_SIZE 64
void print_vec(__m128 vec) {
    float temp[4];
    _mm_store_ps(temp, vec);
    printf("[%f, %f, %f, %f]\n", temp[0], temp[1], temp[2], temp[3]);
}
void transpose(float** A)
{
    for (int i = 0; i < N; i++)
        for (int j = i + 1; j < N; j++)
            std::swap(A[i][j], A[j][i]);
}
void matmul_seq(float** A, float** B, float** C) {
  float sum = 0;
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      sum = 0;
      for (int k = 0; k < N; k++) {
        sum += A[i][k] * B[k][j];
      }
      C[i][j] = sum;
    }
  }
}

void matmul_sse4(float** A, float** B, float** C) {
  for (int i = 0; i < N; i ++) {
        for (int j = 0; j < N; j+=4) {
            __m128 sum = _mm_setzero_ps();
            for (int k = 0; k < N; k++) {
              //Broadcasting value of A[i][k]
                __m128 a = _mm_set1_ps(A[i][k]);
                //Loading first four items of B
                __m128 b = _mm_loadu_ps(&B[k][j]);
                sum = _mm_add_ps(sum, _mm_mul_ps(a, b));
            }
            _mm_storeu_ps(&C[i][j], sum);
        }
    }
}

void matmul_sse4_aligned(float** A, float** B, float** C) {
  for (int i = 0; i < N; i ++) {
        for (int j = 0; j < N; j+=4) {
            __m128 sum = _mm_setzero_ps();
            for (int k = 0; k < N; k++) {
                __m128 a = _mm_set1_ps(A[i][k]);
                // printf("a:::::::::::::::::::::::::::");
                // print_vec(a);
                __m128 b = _mm_load_ps(&B[k][j]);         
                // printf("b:::::::::::::::::::::::::::");
                // print_vec(b);
                sum = _mm_add_ps(sum, _mm_mul_ps(a, b));
            }
            _mm_store_ps(&C[i][j], sum);
        }
    }
}

void matmul_avx21(float** A, float** B, float** C) {
    // Block sizes for tiling
    int blockSizeJ = std::min(512, N); // Maximum size for the outer loop (column-wise)
    int blockSizeK = std::min(16, N);   // Maximum size for the inner loop (row-wise)

    // iterating over columns of B but rows of C
    for (int jBlock = 0; jBlock < N; jBlock += blockSizeJ) {
        // itrate over rows of A , cols of C
        for (int kBlock = 0; kBlock < N; kBlock += blockSizeK) {
            // iterate through each row of A
            for (int i = 0; i < N; i++) {
                // processing blocks of 16 elements at a time for the current row
                for (int j = jBlock; j < jBlock + blockSizeJ; j += 16) {
                    __m256 sumA, sumB; // Vectors to hold intermediate results for 2 rows

                    // Initialize sums to zero for the first block of K
                    if (kBlock == 0) {
                        sumA = sumB = _mm256_setzero_ps();
                    } else {
                        // Load the existing results from C if not the first block
                        sumA = _mm256_loadu_ps(&C[i][j]);
                        sumB = _mm256_loadu_ps(&C[i][j + 8]);
                    }

                    // Determine the limit for the inner loop
                    int limit = std::min(N, kBlock + blockSizeK);
                    // Loop over the inner dimension of the matrices
                    for (int k = kBlock; k < limit; k++) {
                        // Broadcast the value of A[i][k] to all elements of a vector
                        __m256 broadcastA = _mm256_set1_ps(A[i][k]);
                        // Load vectors from B
                        __m256 vecA_B = _mm256_loadu_ps(&B[k][j]);
                        __m256 vecB_B = _mm256_loadu_ps(&B[k][j + 8]);
                         // printf("vecA:::::::::::::::::::::::::::");
                        // print_vec(vecA_B);

                        // Perform the multiplication and accumulate the results
                        sumA = _mm256_add_ps(sumA, _mm256_mul_ps(broadcastA, vecA_B));
                        sumB = _mm256_add_ps(sumB, _mm256_mul_ps(broadcastA, vecB_B));
                    }
                    // Store the accumulated results back to C
                    _mm256_storeu_ps(&C[i][j], sumA);
                    _mm256_storeu_ps(&C[i][j + 8], sumB);
                }
            }
        }
    }
}

void matmul_avx22(float** A, float** B, float** C) {
    // Block sizes for tiling
    int blockSizeJ = std::min(512, N); // Maximum size for the outer loop (column-wise)
    int blockSizeK = std::min(24, N);   // Maximum size for the inner loop (row-wise)

    // iterating over columns of matrix B and rows of matrix C
    for (int jBlock = 0; jBlock < N; jBlock += blockSizeJ) {
        // iterating over rows of A and cols of C
        for (int kBlock = 0; kBlock < N; kBlock += blockSizeK) {
            // processing two rows of A at a time
            for (int i = 0; i < N; i += 2) {
                // processing blocks of 16 elements at a time for the current row
                for (int j = jBlock; j < jBlock + blockSizeJ; j += 16) {
                    __m256 sumA1, sumB1, sumA2, sumB2; // Vectors to hold intermediate results for two rows

                    // Initialize sums to zero for the first block of K
                    if (kBlock == 0) {
                        sumA1 = sumB1 = sumA2 = sumB2 = _mm256_setzero_ps();
                    } else {
                        // load the existing results from matrix C if not the first block
                        sumA1 = _mm256_loadu_ps(&C[i][j]);
                        sumB1 = _mm256_loadu_ps(&C[i][j + 8]);
                        sumA2 = _mm256_loadu_ps(&C[i + 1][j]);
                        sumB2 = _mm256_loadu_ps(&C[i + 1][j + 8]);
                    }

                    // the limit for the inner loop
                    int limit = std::min(N, kBlock + blockSizeK);
                    // Loop over the inner dimension of the matrices
                    for (int k = kBlock; k < limit; k++) {
                        // broadcast the value of A[i][k] to all elements of the vector
                        __m256 broadcastA1 = _mm256_set1_ps(A[i][k]);
                        // Load vectors from matrix B
                        __m256 vecA_B = _mm256_loadu_ps(&B[k][j]);
                        __m256 vecB_B = _mm256_loadu_ps(&B[k][j + 8]);

                        // perform the multiplication & accumulate the results forthe first row
                        sumA1 = _mm256_add_ps(sumA1, _mm256_mul_ps(broadcastA1, vecA_B));
                        sumB1 = _mm256_add_ps(sumB1, _mm256_mul_ps(broadcastA1, vecB_B));

                        // broadcast the value of A[i + 1][k] for the second row
                        __m256 broadcastA2 = _mm256_set1_ps(A[i + 1][k]);
                        // accumulate results for the second row
                        sumA2 = _mm256_add_ps(sumA2, _mm256_mul_ps(broadcastA2, vecA_B));
                        sumB2 = _mm256_add_ps(sumB2, _mm256_mul_ps(broadcastA2, vecB_B));
                    }

                    // Store the accumulated results back to matrix C
                    _mm256_storeu_ps(&C[i][j], sumA1);
                    _mm256_storeu_ps(&C[i][j + 8], sumB1);
                    _mm256_storeu_ps(&C[i + 1][j], sumA2);
                    _mm256_storeu_ps(&C[i + 1][j + 8], sumB2);
                }
            }
        }
    }
}

void matmul_avx2(float** A, float** B, float** C) {
     for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j += 8) { // Process 8 elements at a time
            __m256 sum = _mm256_setzero_ps(); // Initialize the sum to zero

            for (int k = 0; k < N; k++) {
                __m256 a = _mm256_set1_ps(A[i][k]); // Broadcast A[i][k] to all elements of the vector
                __m256 b = _mm256_loadu_ps(&B[k][j]); // Load 8 elements from B
                sum = _mm256_add_ps(sum, _mm256_mul_ps(a, b)); // Multiply and accumulate
            }

            // Store the result into C
            _mm256_storeu_ps(&C[i][j], sum);
        }
    }
  
}

void matmul_avx2_aligned(float** A, float** B, float** C) {
     for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j += 8) { // Process 8 elements at a time
            __m256 sum = _mm256_setzero_ps(); // Initialize the sum to zero

            for (int k = 0; k < N; k++) {
                __m256 a = _mm256_set1_ps(A[i][k]); // Broadcast A[i][k] to all elements of the vector
                __m256 b = _mm256_load_ps(&B[k][j]); // Load 8 elements from B
                sum = _mm256_add_ps(sum, _mm256_mul_ps(a, b)); // Multiply and accumulate
            }

            // Store the result into C
            _mm256_store_ps(&C[i][j], sum);
        }
    }
  
}

void check_result(float** w_ref, float** w_opt) {
  float maxdiff = 0.0;
  int numdiffs = 0;

  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      float this_diff = w_ref[i][j] - w_opt[i][j];
      if (fabs(this_diff) > EPSILON) {
        numdiffs++;
        if (this_diff > maxdiff)
          maxdiff = this_diff;
      }
    }
  }

  if (numdiffs > 0) {
    cout << numdiffs << " Diffs found over THRESHOLD " << EPSILON
         << "; Max Diff = " << maxdiff << endl;
  } else {
    cout << "No differences found between base and test versions\n";
  }
}

int main() {
  auto** A = new float*[N];
  for (int i = 0; i < N; i++) {
    A[i] = new float[N]();
  }
  auto** B = new float*[N];
  for (int i = 0; i < N; i++) {
    B[i] = new float[N]();
  }

  auto** C_seq = new float*[N];
  auto** C_sse4 = new float*[N];
  auto** C_avx2 = new float*[N];
  for (int i = 0; i < N; i++) {
    C_seq[i] = new float[N]();
    C_sse4[i] = new float[N]();
    C_avx2[i] = new float[N]();
  }

  // initialize arrays
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < N; j++) {
      A[i][j] = (float)(i-j*2)+6.5f;
      B[i][j] = (float)(3*i-j)-0.8f;
      C_seq[i][j] = 0.0F;
      C_sse4[i][j] = 0.0F;
      C_avx2[i][j] = 0.0F;
    }
  }

  HRTimer start = HR::now();
  matmul_seq(A, B, C_seq);
  HRTimer end = HR::now();
  auto duration = duration_cast<milliseconds>(end - start).count();
  cout << "Matmul seq time: " << duration << " ms" << endl;
  // printf("A:::::::::::::::::::\n");
  // for (int i = 0; i < N; i++) {
  //   for (int j = 0; j < N; j++) {
  //     printf("%f\t", C_seq[i][j]);
  //   }
  //   printf("\n");
  // }
  // printf("B:::::::::::::::::::\n");
  // for (int i = 0; i < N; i++) {
  //   for (int j = 0; j < N; j++) {
  //     printf("%f\t", B[i][j]);
  //   }
  //   printf("\n");
  // }
  
  // transpose(B);
  // printf("B^T:::::::::::::::::::\n");
  // for (int i = 0; i < N; i++) {
  //   for (int j = 0; j < N; j++) {
  //     printf("%f\t", B[i][j]);
  //   }
  //   printf("\n");
  // }

  // printf("Here:::::::::::;");

  float** A_aligned = static_cast<float**>(aligned_alloc(16,sizeof(float*)*N));
  for(int i=0; i<N; i++){
    A_aligned[i] = static_cast<float*>(aligned_alloc(16,sizeof(float)*N));
  }
  for(int i=0; i<N; i++){
    for(int j=0; j<N; j++){
      A_aligned[i][j] = A[i][j];
    }
  }

  float** B_aligned = static_cast<float**>(aligned_alloc(16,sizeof(float*)*N));
  for(int i=0; i<N; i++){
    B_aligned[i] = static_cast<float*>(aligned_alloc(16,sizeof(float)*N));
  }
  for(int i=0; i<N; i++){
    for(int j=0; j<N; j++){
      B_aligned[i][j] = B[i][j];
    }
  }
  float** C_aligned = static_cast<float**>(aligned_alloc(16,sizeof(float*)*N));
  for(int i=0; i<N; i++){
    C_aligned[i] = static_cast<float*>(aligned_alloc(16,sizeof(float)*N));
  }
  for(int i=0; i<N; i++){
    for(int j=0; j<N; j++){
      C_aligned[i][j] = 0.0f;
    }
  }

  // for (int i = 0; i < N; i++) {
  //   for (int j = 0; j < N; j++) {
  //     printf("%f\t", C_aligned[i][j]);
  //   }
  //   printf("\n");
  // }


  // auto** B = new float*[N];
  // for (int i = 0; i < N; i++) {
  //   B[i] = new float[N]();
  // }

  // auto** C_seq = new float*[N];
  
  start = HR::now();
  matmul_sse4(A, B, C_sse4);
  end = HR::now();
  check_result(C_seq, C_sse4);
  duration = duration_cast<milliseconds>(end - start).count();
  cout << "Matmul SSE4 time: " << duration << " ms" << endl;

  
  start = HR::now();
  matmul_sse4_aligned(A_aligned, B_aligned, C_aligned);
  end = HR::now();
  check_result(C_seq, C_aligned);
  duration = duration_cast<milliseconds>(end - start).count();
  cout << "Matmul SSE4 time (Aligned): " << duration << " ms" << endl;

  start = HR::now();
  matmul_avx2(A, B, C_avx2);
  end = HR::now();
  check_result(C_seq, C_avx2);
  duration = duration_cast<milliseconds>(end - start).count();
  cout << "Matmul AVX2 time: " << duration << " ms" << endl;

  start = HR::now();
  matmul_avx2(A_aligned, B_aligned, C_aligned);
  end = HR::now();
  check_result(C_seq, C_aligned);
  duration = duration_cast<milliseconds>(end - start).count();
  cout << "Matmul AVX2 time (Aligned): " << duration << " ms" << endl;


  start = HR::now();
  matmul_avx21(A, B, C_avx2);
  end = HR::now();
  check_result(C_seq, C_avx2);
  duration = duration_cast<milliseconds>(end - start).count();

  cout << "Matmul AVX21 time: " << duration << " ms" << endl;
    start = HR::now();
  matmul_avx22(A, B, C_avx2);
  end = HR::now();
  check_result(C_seq, C_avx2);
  duration = duration_cast<milliseconds>(end - start).count();
  cout << "Matmul AVX22 time: " << duration << " ms" << endl;

  return EXIT_SUCCESS;
}