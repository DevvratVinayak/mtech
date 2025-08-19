#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>

using std::cout;
using std::endl;
using std::chrono::duration_cast;
using HR = std::chrono::high_resolution_clock;
using HRTimer = HR::time_point;
using std::chrono::microseconds;
using std::chrono::milliseconds;

const static float EPSILON = std::numeric_limits<float>::epsilon();

#define N (1024)

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

void matmul_sse4(float** A, float** B, float** C) {}

void matmul_avx2(float** A, float** B, float** C) {}

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
      A[i][j] = 0.1F;
      B[i][j] = 0.2F;
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

  start = HR::now();
  matmul_sse4(A, B, C_sse4);
  end = HR::now();
  check_result(C_seq, C_sse4);
  duration = duration_cast<milliseconds>(end - start).count();
  cout << "Matmul SSE4 time: " << duration << " ms" << endl;

  start = HR::now();
  matmul_avx2(A, B, C_avx2);
  end = HR::now();
  check_result(C_seq, C_avx2);
  duration = duration_cast<milliseconds>(end - start).count();
  cout << "Matmul AVX2 time: " << duration << " ms" << endl;

  return EXIT_SUCCESS;
}
