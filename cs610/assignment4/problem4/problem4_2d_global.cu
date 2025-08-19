#include <cstdlib>
#include <cuda.h>
#include <iostream>
#include <numeric>
#include <sys/time.h>
#include <assert.h>
#include <limits>

#define THRESHOLD (std::numeric_limits<float>::epsilon())
// #define N 8
using std::cerr;
using std::cout;
using std::endl;

#define cudaCheckError(ans)                                                    \
  { gpuAssert((ans), __FILE__, __LINE__); }
inline void gpuAssert(cudaError_t code, const char* file, int line,
                      bool abort = true) {
  if (code != cudaSuccess) {
    fprintf(stderr, "GPUassert: %s %s %d\n", cudaGetErrorString(code), file,
            line);
    if (abort)
      exit(code);
  }
}

const uint64_t N = (512);
#define     MASK_WIDTH      7

__host__ void convolution2D(double* input, double* h_conv, double* output) {
    int pad = MASK_WIDTH / 2; // Padding for the kernel

    // Initialize the output array to zeros
    for (int i = 0; i < N * N; ++i) {
        output[i] = 0.0f;
    }

    // Perform convolution for all positions
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            double sum = 0.0f;

            // Apply the kernel
            for (int k = 0; k < MASK_WIDTH; ++k) {
                for (int l = 0; l < MASK_WIDTH; ++l) {
                    // Calculate the corresponding input index
                    int inputRow = i + k - pad;
                    int inputCol = j + l - pad;

                    // If the input index is out of bounds, treat it as 0
                    if (inputRow >= 0 && inputRow < N && inputCol >= 0 && inputCol < N) {
                        sum += input[inputRow * N + inputCol] * h_conv[k * MASK_WIDTH + l];
                    }
                }
            }
            output[i * N + j] = sum;
        }
    }
}


// TODO: Edit the function definition as required
__global__ void kernel2D(double* d_input, double* d_conv, double* d_output) {
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    int row = blockIdx.y * blockDim.y + threadIdx.y;

    int half_mask = MASK_WIDTH / 2;

    if (col < N && row < N) {
        double sum = 0.0f;

        for (int m = -half_mask; m <= half_mask; m++) {
            for (int n = -half_mask; n <= half_mask; n++) {
                int input_row = row + m;
                int input_col = col + n;

                if (input_row >= 0 && input_row < N && input_col >= 0 && input_col < N) {
                    double input_value = d_input[input_row * N + input_col];
                    double conv_value = d_conv[(m + half_mask) * MASK_WIDTH + (n + half_mask)];
                    sum += input_value * conv_value;
                }
            }
        }

        d_output[row * N + col] = sum;
    }
}


__host__ void check_result(const double* w_ref, const double* w_opt) {
  double maxdiff = 0.0;
  int numdiffs = 0;

  for (uint64_t i = 0; i < N; i++) {
    for (uint64_t j = 0; j < N; j++) {
        double this_diff =
            w_ref[i * N + j] - w_opt[i * N + j];
        if (std::fabs(this_diff) > THRESHOLD) {
          numdiffs++;
          if (this_diff > maxdiff) {
            maxdiff = this_diff;
          }
        }
    }
  }

  if (numdiffs > 0) {
    cout << numdiffs << " Diffs found over THRESHOLD " << THRESHOLD
         << "; Max Diff = " << maxdiff << endl;
  } else {
    cout << "No differences found between base and test versions\n";
  }
}

void print2D(const double* A) {
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      cout << A[i * N + j] << "\t";
    }
    cout << "n";
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

__host__ void initializeConvFilter(double* conv){
  for(int i=0; i<MASK_WIDTH*MASK_WIDTH; i++)
    conv[i] = double(1)/double(MASK_WIDTH*MASK_WIDTH);
  // for(int i=0; i<MASK_WIDTH*MASK_WIDTH; i++)
  //   printf("%f\t", conv[i]);
  
}

int main() {


  double *h_input, *h_output, *h_conv, *serial_output;
  double *d_input, *d_output, *d_conv;

	// cpu and gpu memory allocation
	h_input = (double*)malloc(N * N * sizeof(double));
	h_conv = (double *)malloc(MASK_WIDTH * MASK_WIDTH * sizeof(double));		// mean filter
	h_output = (double*)malloc( N* N * sizeof(double));
  serial_output = (double*)malloc( N* N * sizeof(double));

  initializeConvFilter(h_conv);
  

	cudaError_t result1 = cudaMalloc(&d_input,N * N * sizeof(double));
	cudaError_t result2 = cudaMalloc(&d_conv, MASK_WIDTH * MASK_WIDTH * sizeof(double));
	cudaError_t result3 = cudaMalloc(&d_output,N * N * sizeof(double));
  if (result1 != cudaSuccess) {
              std::cerr << "Error copying data result1: " << cudaGetErrorString(result1) << std::endl;
  }
  if (result2 != cudaSuccess) {
        std::cerr << "Error copying data result2: " << cudaGetErrorString(result2) << std::endl;
  }

if (result3 != cudaSuccess) {
        std::cerr << "Error copying data result3: " << cudaGetErrorString(result3) << std::endl;
  }

	assert(result1 == cudaSuccess || result2 == cudaSuccess || result3 == cudaSuccess);

	// set values to cpu memory
	for (int i = 0; i < N*N; i++)
		h_input[i] = (double)(rand()%N);
  cudaEvent_t start, stop;
  
  
  startTimer(start, stop);
  convolution2D(h_input, h_conv, serial_output);
  float serial_time = stopTimer(start, stop);
  std::cout << "Serial time (ms): " << serial_time << "\n"; 



	result1 = cudaMemcpy(d_input, h_input, N * N * sizeof(double), cudaMemcpyHostToDevice);
	result2 = cudaMemcpy(d_conv, h_conv, MASK_WIDTH * MASK_WIDTH * sizeof(double), cudaMemcpyHostToDevice);
	result3 = cudaMemcpy(d_output, h_output, N * N * sizeof(double), cudaMemcpyHostToDevice);
    if (result1 != cudaSuccess) {
              std::cerr << "Error copying data result1: " << cudaGetErrorString(result1) << std::endl;
  }
  if (result2 != cudaSuccess) {
        std::cerr << "Error copying data result2: " << cudaGetErrorString(result2) << std::endl;
  }

if (result3 != cudaSuccess) {
        std::cerr << "Error copying data result3: " << cudaGetErrorString(result3) << std::endl;
  }

	assert(result1 == cudaSuccess || result2 == cudaSuccess || result3 == cudaSuccess);

  int threadsPerBlock = 16;

	int gridCols = ceil(N / double(threadsPerBlock));
	int gridRows = ceil(N / double(threadsPerBlock));

	dim3 gridDim(gridCols, gridRows);
	dim3 blockDim(threadsPerBlock, threadsPerBlock);	


  // TODO: Fill in kernel2D
  startTimer(start, stop);
	kernel2D << < gridDim, blockDim >> > ((double*)d_input, (double*)d_conv, (double*)d_output);

	// Device -> Host
	cudaError_t result = cudaMemcpy(h_output, d_output, N * N * sizeof(double), cudaMemcpyDeviceToHost);
	assert(result == cudaSuccess);
  float kernel_time = stopTimer(start,stop);
	cudaDeviceSynchronize();

  // TODO: Adapt check_result() and invoke
  check_result(h_output, serial_output);
  // for(int i=0; i<60; i++){
  //   printf("%f\t%f\n", h_output[i], serial_output[i]);
  // }
  // float kernel_time;
  std::cout << "Kernel time with global memory (2D) (ms): " << kernel_time << "\n";
	// cpu and gpu memory free
  cudaFree(d_input);
  cudaFree(d_conv);
  cudaFree(d_output);

  return EXIT_SUCCESS;
}
//nvcc -O2 -std=c++17 -allow-unsupported-compiler -arch=sm_80 -lineinfo -res-usage -src-in-ptx problem4_2d_global.cu -o prob4.out

//export PATH=/usr/local/cuda-11.8/bin:$PATH