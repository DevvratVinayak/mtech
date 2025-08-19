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

const uint64_t N = (128);
#define MASK_WIDTH 11

__host__ void convolution3D(double* input, double* h_conv, double* output) {
    // Calculate the offset for the mask
    int offset = MASK_WIDTH / 2;

    // Initialize the output array with zeros
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            for (int k = 0; k < N; k++) {
                output[i * N * N + j * N + k] = 0.0;
            }
        }
    }

    // Perform 3D convolution
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            for (int k = 0; k < N; k++) {
                double sum = 0.0;

                // Iterate over the kernel
                for (int di = -offset; di <= offset; di++) {
                    for (int dj = -offset; dj <= offset; dj++) {
                        for (int dk = -offset; dk <= offset; dk++) {
                            // Calculate the corresponding indices in the input array
                            int ni = i + di;
                            int nj = j + dj;
                            int nk = k + dk;

                            // Check if indices are within bounds
                            if (ni >= 0 && ni < N && nj >= 0 && nj < N && nk >= 0 && nk < N) {
                                sum += input[ni * N * N + nj * N + nk] * 
                                       h_conv[(di + offset) * MASK_WIDTH * MASK_WIDTH + (dj + offset) * MASK_WIDTH + (dk + offset)];
                            }
                        }
                    }
                }

                // Assign the computed value to the output array
                output[i * N * N + j * N + k] = sum;
            }
        }
    }
}



// TODO: Edit the function definition as required
__global__ void kernel3D(double* d_input, double* d_conv, double* d_output) {
    // Calculate the offset for the mask
    int offset = MASK_WIDTH / 2;
    // printf("Here::::::::::::222");
    // Get the indices for the current thread
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    int j = blockIdx.y * blockDim.y + threadIdx.y;
    int k = blockIdx.z * blockDim.z + threadIdx.z;

    // Check if the indices are within bounds
    if (i < N && j < N && k < N) {
        double sum = 0.0;

        // Perform convolution
        for (int di = -offset; di <= offset; di++) {
            for (int dj = -offset; dj <= offset; dj++) {
                for (int dk = -offset; dk <= offset; dk++) {
                    int ni = i + di;
                    int nj = j + dj;
                    int nk = k + dk;

                    // Check if indices are within bounds
                    if (ni >= 0 && ni < N && nj >= 0 && nj < N && nk >= 0 && nk < N) {
                        sum += d_input[ni * N * N + nj * N + nk] *
                               d_conv[(di + offset) * MASK_WIDTH * MASK_WIDTH + (dj + offset) * MASK_WIDTH + (dk + offset)];
                    }
                }
            }
        }

        // Assign the computed value to the output array
        d_output[i * N * N + j * N + k] = sum;
    }
}

__host__ void check_result_3D(const double* w_ref, const double* w_opt) {
  double maxdiff = 0.0;
  int numdiffs = 0;
//   printf("Here::::::::::::");

  for (uint64_t i = 0; i < N; i++) {
    for (uint64_t j = 0; j < N; j++) {
      for(uint64_t k = 0; k < N; k++){
        double this_diff = w_ref[i * N * N + j * N + k] - w_opt[i * N * N + j * N + k];
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
    cout << numdiffs << " Diffs found over THRESHOLD " << THRESHOLD
         << "; Max Diff = " << maxdiff << endl;
  } else {
    cout << "No differences found between base and test versions\n";
  }
}

void print3D(const double* A) {
  for (int i = 0; i < N; ++i) {
    for (int j = 0; j < N; ++j) {
      for (int k = 0; k < N; ++k) {
        cout << A[i * N * N + j * N + k] << "\t";
      }
      cout << "\n";
    }
    cout << "\n";
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
  for(int i=0; i<MASK_WIDTH*MASK_WIDTH*MASK_WIDTH; i++)
    conv[i] = double(1)/double(MASK_WIDTH*MASK_WIDTH*MASK_WIDTH);
  // for(int i=0; i<MASK_WIDTH*MASK_WIDTH*MASK_WIDTH; i++)
  //   printf("%f\t", conv[i]);
  
}
int main() {

    double *h_input, *h_output, *h_conv, *serial_output;
    double *d_input, *d_output, *d_conv;

	// cpu and gpu memory allocation
	h_input = (double*)malloc(N * N * N * sizeof(double));
	h_conv = (double*)malloc(MASK_WIDTH * MASK_WIDTH * MASK_WIDTH * sizeof(double));
	h_output = (double*)malloc( N * N * N * sizeof(double));
    serial_output = (double*)malloc( N * N * N * sizeof(double));

  initializeConvFilter(h_conv);
	cudaError_t result1 = cudaMalloc(&d_input,N * N * N * sizeof(double));
    cudaError_t result2 = cudaMalloc(&d_conv, MASK_WIDTH * MASK_WIDTH * MASK_WIDTH * sizeof(double));
	cudaError_t result3 = cudaMalloc(&d_output,N * N * N * sizeof(double));
	assert(result1 == cudaSuccess || result2 == cudaSuccess || result3 == cudaSuccess);
    cudaEvent_t start, stop;
	// set values to cpu memory
	for (int i = 0; i < N*N*N; i++)
		h_input[i] = (double)(rand()%N);
    // print3D(h_input);
  
  
    startTimer(start, stop);
    convolution3D(h_input, h_conv, serial_output);
    float serial_time = stopTimer(start, stop);
    std::cout << "Serial time (3D) (ms): " << serial_time << "\n"; 
    // print3D(serial_output);

	result1 = cudaMemcpy(d_input, h_input, N * N * N * sizeof(double), cudaMemcpyHostToDevice);
	result2 = cudaMemcpy(d_conv, h_conv, MASK_WIDTH * MASK_WIDTH * MASK_WIDTH * sizeof(double), cudaMemcpyHostToDevice);
	result3 = cudaMemcpy(d_output, h_output, N * N * N * sizeof(double), cudaMemcpyHostToDevice);
	assert(result1 == cudaSuccess || result2 == cudaSuccess || result3 == cudaSuccess);

  int threadsPerBlock = 8;

	int gridCols = ceil(N / double(threadsPerBlock));
	int gridRows = ceil(N / double(threadsPerBlock));
    int gridDepth = ceil(N / double(threadsPerBlock));

	dim3 gridDim(gridCols, gridRows, gridDepth);
	dim3 blockDim(threadsPerBlock, threadsPerBlock, threadsPerBlock);	

  startTimer(start, stop);
	kernel3D << < gridDim, blockDim >> > ((double*)d_input, (double*)d_conv, (double*)d_output);

    // printf("Here::::::::::::333333");

	// Device -> Host
	cudaError_t result = cudaMemcpy(h_output, d_output, N * N * N * sizeof(double), cudaMemcpyDeviceToHost);
	assert(result == cudaSuccess);

    float kernel_time = stopTimer(start,stop);
	cudaDeviceSynchronize();
  // printf("--------------------------------------\n");
  // print3D(h_output);
  // TODO: Adapt check_result() and invoke
  check_result_3D(h_output, serial_output);
  // float kernel_time;
    std::cout << "Kernel time (3D) (ms): " << kernel_time << "\n";
	// cpu and gpu memory free

    /* Freeing CPU and GPU Memory */
    // for(int i=0; i<40; i++)
    //     printf("serial = %f,  kernel = %f\n", serial_output[i], h_output[i]);
    // printf("\n---------------------------------------------\n");
    // printf(h_output[0][0][0]);

	result1 = cudaFree(d_input);
	result2 = cudaFree(d_output);
	result3 = cudaFree(d_conv);
	assert(result1 == cudaSuccess || result2 == cudaSuccess || result3 == cudaSuccess);

	free(h_input);
	free(h_output);
	free(h_conv);

  return EXIT_SUCCESS;
}
//nvcc -O2 -std=c++17 -allow-unsupported-compiler -arch=sm_80 -lineinfo -res-usage -src-in-ptx problem4_3d_global.cu -o prob4.out
