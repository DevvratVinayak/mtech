#include <cstdlib>
#include <cuda.h>
#include <iostream>
#include <numeric>
#include <sys/time.h>
#include <assert.h>
#include <limits>

#define THRESHOLD (std::numeric_limits<float>::epsilon())
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
#define     MASK_WIDTH      11
#define     MASK_RADIUS     MASK_WIDTH / 2
#define     TILE_WIDTH      8


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
                              // printf("%f\t%f\n", input[ni * N * N + nj * N + nk], h_conv[(di + offset) * MASK_WIDTH * MASK_WIDTH + (dj + offset) * MASK_WIDTH + (dk + offset)]);
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

#define BLOCK_SIZE TILE_WIDTH + MASK_WIDTH - 1


__global__ void kernel3D_shared(double *d_input,double *d_conv, double *d_output) { 
  
  __shared__ float shared[BLOCK_SIZE][BLOCK_SIZE][BLOCK_SIZE];

  int tid_x = threadIdx.x; 
  int tid_y = threadIdx.y; 
  int tid_z = threadIdx.z; 
  
  int x = blockIdx.x * TILE_WIDTH + tid_x; 
  int y = blockIdx.y * TILE_WIDTH + tid_y; 
  int z = blockIdx.z * TILE_WIDTH + tid_z;
  
  if((x >= 0 ) && (x < N) && (y >= 0) && (y < N) && (z >= 0) && (z < N))
  { 
    double sum = 0;
    
   for (int k = 0; k < MASK_WIDTH; k++)
   {
     for(int j = 0; j < MASK_WIDTH; j++)
     {
       for(int i = 0; i < MASK_WIDTH; i++)
       {
          int x1 = x - MASK_RADIUS + i; 
         int y1 = y - MASK_RADIUS + j; 
         int z1 = z - MASK_RADIUS + k; 
          if((x1 >= 0 ) && (x1 < N) && (y1 >= 0) && (y1 < N) && (z1 >= 0) && (z1 < N))
          {
            shared[tid_z][tid_y][tid_x] = d_input[(z1 * N * N) + (y1 * N) + x1];
            sum += d_conv[(k * MASK_WIDTH * MASK_WIDTH) + (j * MASK_WIDTH) + i] * shared[tid_z][tid_y][tid_x]; 
          }
           else 
           {
             shared[tid_z][tid_y][tid_x] = 0.0f;
           }
           __syncthreads(); 
       
       }
     }
   }
    d_output[(z * N * N) + (y * N) + x] = sum;  
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
		h_input[i] = (double)(i+890);
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

  int gridCols = ceil(N / double(TILE_WIDTH));
	int gridRows = ceil(N / double(TILE_WIDTH));
    int gridDepth = ceil(N / double(TILE_WIDTH));

    dim3 blockDim(TILE_WIDTH, TILE_WIDTH, TILE_WIDTH);
    dim3 gridDim(gridCols, gridRows, gridDepth);
    startTimer(start, stop);
  //Edit here::::::::::::::::::::::::::::::::::::::::::::::::::
	kernel3D_shared << < gridDim, blockDim >> > ((double*)d_input, (double*)d_conv, (double*)d_output);

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
    std::cout << "Kernel time (3D) with shared memory (ms): " << kernel_time << "\n";
	// cpu and gpu memory free

    // check_result_3D(h_output, serial_output);

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


  // // TODO: Fill in kernel3D
  // // TODO: Adapt check_result() and invoke
  // std::cout << "Kernel3D time (ms): " << kernel_time << "\n";

  // // TODO: Free memory

  return EXIT_SUCCESS;
}

// nvcc -O2 -std=c++17 -allow-unsupported-compiler -arch=sm_80 -lineinfo -res-usage -src-in-ptid_x problem4_3d_shared.cu -o prob4.out