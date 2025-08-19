// nvcc -ccbin /usr/bin/g++-10 -lineinfo -res-usage -arch=sm\_80 -std=c++11 190997-prob1.cu -o 190997-prob1

#include <cmath>
#include <cstdlib>
#include <cuda.h>
#include <iostream>
#include <sys/time.h>
#include <algorithm>
#include <limits>
const uint64_t N = (512);
#define THRESHOLD (std::numeric_limits<double>::epsilon())

const uint64_t MAX_VAL = 1e6;
const int TILE_SIZE = 16;
const int BLOCK_HEIGHT = 8;

using std::cerr;
using std::cout;
using std::endl;

#define cudaCheckError(ans)                                                                        \
  { gpuAssert((ans), __FILE__, __LINE__); }
inline void gpuAssert(cudaError_t code, const char* file, int line, bool abort = true) {
  if (code != cudaSuccess) {
    fprintf(stderr, "GPUassert: %s %s %d\n", cudaGetErrorString(code), file, line);
    if (abort)
      exit(code);
  }
}

// naive CUDA kernel
__global__ void kernel1(const double *input, double *output) {
   int blockHeight = blockDim.y;
  int x_coord = blockIdx.x * TILE_SIZE + threadIdx.x;
  int y_coord = blockIdx.y * TILE_SIZE + threadIdx.y;
  int z_coord = blockIdx.z * blockDim.z + threadIdx.z;
  
  for (int j = 0; j < TILE_SIZE; j += blockHeight) {
    if (x_coord > 0 && x_coord < N - 1 && y_coord + j > 0 && y_coord + j < N - 1 && z_coord > 0 && z_coord < N - 1) {
      int index = z_coord * N * N + (y_coord + j) * N + x_coord;
      output[index] = 0.8 * (input[(z_coord - 1) * N * N + (y_coord + j) * N + x_coord]
                          + input[(z_coord + 1) * N * N + (y_coord + j) * N + x_coord]
                          + input[z_coord * N * N + (y_coord + j - 1) * N + x_coord]
                          + input[z_coord * N * N + (y_coord + j + 1) * N + x_coord]
                          + input[z_coord * N * N + (y_coord + j) * N + (x_coord - 1)]
                          + input[z_coord * N * N + (y_coord + j) * N + (x_coord + 1)]);
    }
  }
}

__global__ void kernel2(const double *input, double *output) {
  __shared__ double sharedTile[TILE_SIZE * TILE_SIZE * BLOCK_HEIGHT];

  int blockHeight = blockDim.y;
  int x_coord = blockIdx.x * TILE_SIZE + threadIdx.x;
  int y_coord = blockIdx.y * TILE_SIZE + threadIdx.y;
  int z_coord = blockIdx.z * blockDim.z + threadIdx.z;
  
  for (int j = 0; j < TILE_SIZE; j += blockHeight) {
    if (x_coord > 0 && x_coord < N - 1 && y_coord + j > 0 && y_coord + j < N - 1 && z_coord > 0 && z_coord < N - 1) {
      int index = threadIdx.z * TILE_SIZE * TILE_SIZE
              + (threadIdx.y + j) * TILE_SIZE
              + threadIdx.x;
      sharedTile[index] = 0.8 * (input[(z_coord - 1) * N * N + (y_coord + j) * N + x_coord]
                               + input[(z_coord + 1) * N * N + (y_coord + j) * N + x_coord]
                               + input[z_coord * N * N + (y_coord + j - 1) * N + x_coord]
                               + input[z_coord * N * N + (y_coord + j + 1) * N + x_coord]
                               + input[z_coord * N * N + (y_coord + j) * N + (x_coord - 1)]
                               + input[z_coord * N * N + (y_coord + j) * N + (x_coord + 1)]);
    }
  }

  __syncthreads();

  for (int j = 0; j < TILE_SIZE; j += blockHeight) {
    if (x_coord > 0 && x_coord < N - 1 && y_coord + j > 0 && y_coord + j < N - 1 && z_coord > 0 && z_coord < N - 1) {
      int index = threadIdx.z * TILE_SIZE * TILE_SIZE
              + (threadIdx.y + j) * TILE_SIZE
              + threadIdx.x;
      output[z_coord * N * N + (y_coord + j) * N + x_coord] = sharedTile[index];
    }
  }
}

__global__ void kernel2_unroll(const double *input, double *output) {
  __shared__ double sharedTile[TILE_SIZE * TILE_SIZE * BLOCK_HEIGHT];

  int blockHeight = blockDim.y;
  int x_coord = blockIdx.x * TILE_SIZE + threadIdx.x;
  int y_coord = blockIdx.y * TILE_SIZE + threadIdx.y;
  int z_coord = blockIdx.z * blockDim.z + threadIdx.z;
  
  for (int j = 0; j < TILE_SIZE; j += 4*blockHeight) {
    if (x_coord > 0 && x_coord < N - 1 && y_coord + j > 0 && y_coord + j < N - 1 && z_coord > 0 && z_coord < N - 1) {
      int index = threadIdx.z * TILE_SIZE * TILE_SIZE
              + (threadIdx.y + j) * TILE_SIZE
              + threadIdx.x;
      sharedTile[index] = 0.8 * (input[(z_coord - 1) * N * N + (y_coord + j) * N + x_coord]
                               + input[(z_coord + 1) * N * N + (y_coord + j) * N + x_coord]
                               + input[z_coord * N * N + (y_coord + j - 1) * N + x_coord]
                               + input[z_coord * N * N + (y_coord + j + 1) * N + x_coord]
                               + input[z_coord * N * N + (y_coord + j) * N + (x_coord - 1)]
                               + input[z_coord * N * N + (y_coord + j) * N + (x_coord + 1)]);
    }
       if (x_coord > 0 && x_coord < N - 1 && y_coord + (j + 1) > 0 && y_coord + ( j + 1 ) < N - 1 && z_coord > 0 && z_coord < N - 1) {
      int index = threadIdx.z * TILE_SIZE * TILE_SIZE
              + (threadIdx.y + (j + 1)) * TILE_SIZE
              + threadIdx.x;
      sharedTile[index] = 0.8 * (input[(z_coord - 1) * N * N + (y_coord + j + 1) * N + x_coord]
                               + input[(z_coord + 1) * N * N + (y_coord + j  + 1) * N + x_coord]
                               + input[z_coord * N * N + (y_coord + j) * N + x_coord]
                               + input[z_coord * N * N + (y_coord + j + 2) * N + x_coord]
                               + input[z_coord * N * N + (y_coord + j+1) * N + (x_coord - 1)]
                               + input[z_coord * N * N + (y_coord + j+1) * N + (x_coord + 1)]);
    }

    if (x_coord > 0 && x_coord < N - 1 && y_coord + (j + 2) > 0 && y_coord + ( j + 2 ) < N - 1 && z_coord > 0 && z_coord < N - 1) {
      int index = threadIdx.z * TILE_SIZE * TILE_SIZE
              + (threadIdx.y + (j + 2)) * TILE_SIZE
              + threadIdx.x;
      sharedTile[index] = 0.8 * (input[(z_coord - 1) * N * N + (y_coord + j + 2) * N + x_coord]
                               + input[(z_coord + 1) * N * N + (y_coord + j  + 2) * N + x_coord]
                               + input[z_coord * N * N + (y_coord + j + 1) * N + x_coord]
                               + input[z_coord * N * N + (y_coord + j + 3) * N + x_coord]
                               + input[z_coord * N * N + (y_coord + j+2) * N + (x_coord - 1)]
                               + input[z_coord * N * N + (y_coord + j+2) * N + (x_coord + 1)]);
    }


    if (x_coord > 0 && x_coord < N - 1 && y_coord + (j + 3) > 0 && y_coord + ( j + 3 ) < N - 1 && z_coord > 0 && z_coord < N - 1) {
      int index = threadIdx.z * TILE_SIZE * TILE_SIZE
              + (threadIdx.y + (j + 3)) * TILE_SIZE
              + threadIdx.x;
      sharedTile[index] = 0.8 * (input[(z_coord - 1) * N * N + (y_coord + j + 3) * N + x_coord]
                               + input[(z_coord + 1) * N * N + (y_coord + j  + 3) * N + x_coord]
                               + input[z_coord * N * N + (y_coord + j + 2) * N + x_coord]
                               + input[z_coord * N * N + (y_coord + j + 4) * N + x_coord]
                               + input[z_coord * N * N + (y_coord + j+3) * N + (x_coord - 1)]
                               + input[z_coord * N * N + (y_coord + j+3) * N + (x_coord + 1)]);
    }
  }

  __syncthreads();

  for (int j = 0; j < TILE_SIZE; j += blockHeight) {
    if (x_coord > 0 && x_coord < N - 1 && y_coord + j > 0 && y_coord + j < N - 1 && z_coord > 0 && z_coord < N - 1) {
      int index = threadIdx.z * TILE_SIZE * TILE_SIZE
              + (threadIdx.y + j) * TILE_SIZE
              + threadIdx.x;
      output[z_coord * N * N + (y_coord + j) * N + x_coord] = sharedTile[index];
    }
  }
}

__host__ void stencil(const double *in, double *out) {
  for (int i = 1; i < N - 1; i++) {
    for (int j = 1; j < N - 1; j++) {
      for (int k = 1; k < N - 1; k++) {
        out[i * N * N + j * N + k] = 0.8 * (in[(i - 1) * N * N + j * N + k]
                                          + in[(i + 1) * N * N + j * N + k]
                                          + in[i * N * N + (j - 1) * N + k]
                                          + in[i * N * N + (j + 1) * N + k]
                                          + in[i * N * N + j * N + (k - 1)]
                                          + in[i * N * N + j * N + (k + 1)]);
      }
    }
  }
}

__host__ void check_result(const double* w_ref, const double* w_opt, const uint64_t size) {
  double maxdiff = 0.0, this_diff = 0.0;
  int numdiffs = 0;

  for (uint64_t i = 0; i < size; i++) {
    for (uint64_t j = 0; j < size; j++) {
      for (uint64_t k = 0; k < size; k++) {
        this_diff = w_ref[i + N * j + N * N * k] - w_opt[i + N * j + N * N * k];
        if (std::fabs(this_diff) > THRESHOLD) {
          numdiffs++;
          if (this_diff > maxdiff) {
            maxdiff = this_diff;
          }
        }
      }
    }
  }

  if (numdiffs > 0) {
    cout << numdiffs << " Diffs found over THRESHOLD " << THRESHOLD << "; Max Diff = " << maxdiff
         << endl;
  } else {
    cout << "No differences found between base and test versions\n";
  }
}

void print_mat(double* A) {
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      for (int k = 0; k < N; ++k) {
        printf("%lf,", A[i * N * N + j * N + k]);
      }
      printf("      ");
    }
    printf("\n");
  }
}

double rtclock() { // Seconds
  struct timezone Tzp;
  struct timeval Tp;
  int stat;
  stat = gettimeofday(&Tp, &Tzp);
  if (stat != 0) {
    cout << "Error return from gettimeofday: " << stat << "\n";
  }
  return (Tp.tv_sec + Tp.tv_usec * 1.0e-6);
}
float startTimer(cudaEvent_t &start, cudaEvent_t &stop) {
    cudaEventCreate(&start);
    cudaEventCreate(&stop);
    cudaEventRecord(start);
    return 0.0f;
}

float stopTimer(cudaEvent_t start, cudaEvent_t stop) {
    cudaEventRecord(stop);
    cudaEventSynchronize(stop);

    float milliseconds = 0.0f;
    cudaEventElapsedTime(&milliseconds, start, stop);

    cudaEventDestroy(start);
    cudaEventDestroy(stop);

    return milliseconds;
}
int main() {
  uint64_t SIZE = N * N * N;

  double *h_in = static_cast<double *>(malloc(SIZE * sizeof(double)));
  double *h_out_serial = static_cast<double *>(malloc(SIZE * sizeof(double)));
  double *h_out = static_cast<double *>(malloc(SIZE * sizeof(double)));

  for (int i = 0; i < SIZE; i++) {
    h_in[i] = std::rand() % MAX_VAL;
  }
  std::fill_n(h_out_serial, SIZE, 0.0);
  std::fill_n(h_out, SIZE, 0.0);
  printf("======Serial Implementation======\n");
  
  double clkbegin = rtclock();
  stencil(h_in, h_out_serial);
  double clkend = rtclock();
  double cpu_time = (clkend - clkbegin) * 1000;
  cout << "Stencil time (serially): " << cpu_time << " msec" << endl;

  
  printf("======(i) Naive CUDA Implementation======\n");

  double *d_in;
  cudaCheckError(cudaMalloc(&d_in, SIZE * sizeof(double)));
  double *d_out;
  cudaCheckError(cudaMalloc(&d_out, SIZE * sizeof(double)));

  cudaCheckError(cudaMemcpy(d_in, h_in, SIZE * sizeof(double),
                            cudaMemcpyHostToDevice));

  cudaEvent_t start, stop;
  int block_rows = 8;
  dim3 dimBlock(TILE_SIZE, block_rows, BLOCK_HEIGHT);
  dim3 dimGrid(N / TILE_SIZE, N / TILE_SIZE, N / BLOCK_HEIGHT);

  startTimer(start, stop);
  kernel1<<<dimGrid, dimBlock>>>(d_in, d_out);
  float kernel_time = stopTimer(start, stop);
  cudaCheckError(cudaMemcpy(h_out, d_out, SIZE * sizeof(double),
                            cudaMemcpyDeviceToHost));
  check_result(h_out_serial, h_out, N);
  cout << "kernel1 time : " << kernel_time << " msec, Speedup: " << cpu_time / kernel_time << endl << endl;

  printf("======(ii) Shared Memory Tiling======\n");

  int block_row_values[] = {1, 2, 4, 8};
  for (auto block_rows : block_row_values) {
    std::fill_n(h_out, SIZE, 0.0);

    dim3 dimBlock(TILE_SIZE, block_rows, BLOCK_HEIGHT);
    dim3 dimGrid(N / TILE_SIZE, N / TILE_SIZE, N / BLOCK_HEIGHT);

    startTimer(start, stop);
    kernel2<<<dimGrid, dimBlock>>>(d_in, d_out);
        kernel_time = stopTimer(start, stop);
    cudaCheckError(cudaMemcpy(h_out, d_out, SIZE * sizeof(double),
                              cudaMemcpyDeviceToHost));
    check_result(h_out_serial, h_out, N);

    cout << "Block Size = " << block_rows << ", Time: " << kernel_time << " msec, Speedup: " << cpu_time / kernel_time << endl << endl;
  }

  // printf("======(iii) Loop Unrolling + Shared Memory Tiling======\n");
  //   std::fill_n(h_out, SIZE, 0.0);

  //   dim3 dimBlock1(TILE_SIZE, 1, BLOCK_HEIGHT);
  //   dim3 dimGrid1(N / TILE_SIZE, N / TILE_SIZE, N / BLOCK_HEIGHT);

  //   startTimer(start, stop);
  //   kernel2_unroll<<<dimGrid1, dimBlock1>>>(d_in, d_out);
  //   cudaCheckError(cudaMemcpy(h_out, d_out, SIZE * sizeof(double),
  //                             cudaMemcpyDeviceToHost));
  //   kernel_time = stopTimer(start, stop);
  //   check_result(h_out_serial, h_out, N);

  //   cout << "Time (ms): " << kernel_time << "msec, Speedup: " << cpu_time / kernel_time << endl << endl;
  

  printf("====== (iv) Using Pinned Memory======\n");

  double *h_input_pinned;
  cudaCheckError(cudaHostAlloc(&h_input_pinned, SIZE * sizeof(double),
                               cudaHostAllocDefault));
  double *h_output_pinned;
  cudaCheckError(cudaHostAlloc(&h_output_pinned, SIZE * sizeof(double),
                               cudaHostAllocDefault));

  for (int i = 0; i < SIZE; i++) {
    h_input_pinned[i] = h_in[i];
  }
  std::fill_n(h_output_pinned, SIZE, 0.0);

  cudaCheckError(cudaMemcpy(d_in, h_input_pinned, SIZE * sizeof(double),
                            cudaMemcpyHostToDevice));
  startTimer(start, stop);
  kernel2_unroll<<<dimGrid, dimBlock>>>(d_in, d_out);
  kernel_time = stopTimer(start, stop);
  cudaCheckError(cudaMemcpy(h_output_pinned, d_out, SIZE * sizeof(double),
                            cudaMemcpyDeviceToHost));
  check_result(h_out_serial, h_out, N);

  cout << "Kernel 2 (Pinned Memory) time: " << kernel_time << " msec, Speedup: " << cpu_time / kernel_time << endl << endl;

  printf("======(v) Using UVM======\n");
  double *h_input_uvm;
  cudaCheckError(cudaMallocManaged(&h_input_uvm, SIZE * sizeof(double)));
  double *h_output_uvm;
  cudaCheckError(cudaMallocManaged(&h_output_uvm, SIZE * sizeof(double)));
  for (int i = 0; i < SIZE; i++) {
    h_input_uvm[i] = h_in[i];
  }
  std::fill_n(h_output_uvm, SIZE, 0.0);

  startTimer(start, stop);
  kernel2<<<dimGrid, dimBlock>>>(h_input_uvm, h_output_uvm);
  kernel_time = stopTimer(start, stop);
  check_result(h_out_serial, h_output_uvm, N);

  cout << "Kernel 2 (UVM) time: " << kernel_time << " msec, Speedup: " << cpu_time / kernel_time << endl << endl;

  cudaCheckError(cudaFree(d_in));
  cudaCheckError(cudaFree(d_out));
  cudaCheckError(cudaFreeHost(h_input_pinned));
  cudaCheckError(cudaFreeHost(h_output_pinned));
  cudaCheckError(cudaFree(h_input_uvm));
  cudaCheckError(cudaFree(h_output_uvm));

  free(h_in);
  free(h_out_serial);
  free(h_out);

  return EXIT_SUCCESS;
}

//  nvcc -O3 -std=c++17 -allow-unsupported-compiler -arch=sm_80 -lineinfo -res-usage -src-in-ptx problem1.cu -o prob1.out
// nsys nvprof ./prob1.out > problem1.log