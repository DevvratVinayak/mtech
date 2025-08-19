#include <cstdlib>
#include <cuda.h>
#include <iostream>
#include <numeric>
#include <sys/time.h>
#include <assert.h>
#include <limits>
#include <thrust/device_vector.h>
#include <thrust/host_vector.h>
#include <thrust/scan.h>

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


const uint64_t N = 1<<16;

int THREADS_PER_BLOCK = 512;
int ELEMENTS_PER_BLOCK = THREADS_PER_BLOCK * 2;

//Basic Utility Functions
uint64_t nextPowerOfTwo(uint64_t x) {
	uint64_t power = 1;
	while (power < x) {
		power *= 2;
	}
	return power;
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
__host__ void check_result(const uint32_t* w_ref, const uint32_t* w_opt,
                           const uint64_t size) {
  for (uint64_t i = 0; i < size; i++) {
    if (w_ref[i] != w_opt[i]) {
      printf("%d\t%d\t%ld\n", w_ref[i],w_opt[i], i);
      std::cout << "Differences found between the two arrays.\n";
      assert(false);
    }
  }
  std::cout << "No differences found between base and test versions\n";
}
//Kernels

__global__ void add(uint32_t *output, uint64_t size, uint32_t *sums) {
	int block_ID = blockIdx.x;
	int thread_ID = threadIdx.x;
	int  offset = block_ID * size;

	output[offset + thread_ID] += sums[block_ID];
}

__global__ void add(uint32_t *output, uint64_t size, uint32_t *sums1, uint32_t *sums2) {
	int block_ID = blockIdx.x;
	int thread_ID = threadIdx.x;
	int offset = block_ID * size;

	output[offset + thread_ID] += sums1[block_ID] + sums2[block_ID];
}


__global__ void exclusive_sum_large(uint32_t *input, uint32_t *output, uint64_t n, uint32_t *sums) {
	int block_ID = blockIdx.x;
	int thread_ID = threadIdx.x;
	int block_offset = block_ID * n;

	extern __shared__ uint32_t temp[];
	temp[2 * thread_ID] = input[block_offset + (2 * thread_ID)];
	temp[2 * thread_ID + 1] = input[block_offset + (2 * thread_ID) + 1];

	int offset = 1;
	// Up-sweep phase : Accumulating sums in a binary tree structure
	for (int d = n >> 1; d > 0; d >>= 1) 
	{
		__syncthreads();
		if (thread_ID < d)
		{
			int a_i = offset * (2 * thread_ID + 1) - 1;
			int b_i = offset * (2 * thread_ID + 2) - 1;
			temp[b_i] += temp[a_i];
		}
		offset *= 2;
	}
	__syncthreads();


	if (thread_ID == 0) {
		sums[block_ID] = temp[n - 1];
		temp[n - 1] = 0;
	}

	// Down sweep phase: Tracking downwards to calculate the exclusive sum
	for (int d = 1; d < n; d *= 2) 
	{
		offset >>= 1;
		__syncthreads();
		if (thread_ID < d)
		{
			int a_i = offset * (2 * thread_ID + 1) - 1;
			int b_i = offset * (2 * thread_ID + 2) - 1;
			uint32_t swap = temp[a_i];
			temp[a_i] = temp[b_i];
			temp[b_i] += swap;
		}
	}
	__syncthreads();

	output[block_offset + (2 * thread_ID)] = temp[2 * thread_ID];
	output[block_offset + (2 * thread_ID) + 1] = temp[2 * thread_ID + 1];
}

__global__ void cuda_sum_2(uint32_t *input, uint32_t *output, uint64_t n, uint64_t exp) {
	extern __shared__ uint32_t temp[];
	int thread_ID = threadIdx.x;

	if (thread_ID < n) {
		temp[2 * thread_ID] = input[2 * thread_ID]; 
		temp[2 * thread_ID + 1] = input[2 * thread_ID + 1];
	}
	else {
		temp[2 * thread_ID] = 0;
		temp[2 * thread_ID + 1] = 0;
	}


	int offset = 1;
	// Up sweep phase
	for (int d = exp >> 1; d > 0; d >>= 1) 
	{
		__syncthreads();
		if (thread_ID < d)
		{
			int a_i = offset * (2 * thread_ID + 1) - 1;
			int b_i = offset * (2 * thread_ID + 2) - 1;
			temp[b_i] += temp[a_i];
		}
		offset *= 2;
	}

	if (thread_ID == 0) { temp[exp - 1] = 0; } // setting last element to 0

	// Down sweep phase
	for (int d = 1; d < exp; d *= 2) 
	{
		offset >>= 1;
		__syncthreads();
		if (thread_ID < d)
		{
			int a_i = offset * (2 * thread_ID + 1) - 1;
			int b_i = offset * (2 * thread_ID + 2) - 1;
			uint32_t swap = temp[a_i];
			temp[a_i] = temp[b_i];
			temp[b_i] += swap;
		}
	}
	__syncthreads();

	if (thread_ID < n) {
		output[2 * thread_ID] = temp[2 * thread_ID]; 
		output[2 * thread_ID + 1] = temp[2 * thread_ID + 1];
	}
}

//Kernels End

void cuda_sum_large( uint32_t *d_in, uint32_t *d_out, uint64_t size);
void cuda_sum_even(uint32_t *d_in, uint32_t *d_out,  uint64_t size);

void cuda_sum_small(uint32_t *d_in, uint32_t *d_out, uint64_t size) {
	uint64_t exp = nextPowerOfTwo(size);
	cuda_sum_2<< <1, (size + 1) / 2, 2 * exp * sizeof(uint32_t) >> >(d_in, d_out,  size, exp);
}

void cuda_sum_even(uint32_t *d_in, uint32_t *d_out, uint64_t size) {
	const int blocks = size / ELEMENTS_PER_BLOCK;

	uint32_t *sums, *increments;
	cudaMalloc((uint32_t **)&sums, blocks * sizeof(uint32_t));
	cudaMalloc((uint32_t **)&increments, blocks * sizeof(uint32_t));

	exclusive_sum_large<<<blocks, THREADS_PER_BLOCK, 2 * ELEMENTS_PER_BLOCK * sizeof(uint32_t) >>>( d_in, d_out, ELEMENTS_PER_BLOCK, sums);

	const int sum_threads = (blocks + 1) / 2;
	if (sum_threads > THREADS_PER_BLOCK) {
		// perform a large scan on the sums arr
		cuda_sum_large(sums, increments, blocks);
	}
	else {
		cuda_sum_small(sums, increments, blocks);
	}

	add<<<blocks, ELEMENTS_PER_BLOCK>>>(d_out, ELEMENTS_PER_BLOCK, increments);

	cudaFree(sums);
	cudaFree(increments);
}

void cuda_sum_large(uint32_t *d_in, uint32_t *d_out, uint64_t size) {
	uint64_t rem = size % (ELEMENTS_PER_BLOCK);
	if (rem == 0) {
		cuda_sum_even( d_in, d_out, size);
	}
	else {
		// perform a large scan on a truncated length
		uint64_t truncatedLen = size - rem;
		cuda_sum_even( d_in, d_out, truncatedLen);

		uint32_t *start = &(d_out[truncatedLen]);
		cuda_sum_small(&(d_in[truncatedLen]), start, rem);


		add<<<1, rem>>>(start, rem, &(d_in[truncatedLen - 1]), &(d_out[truncatedLen - 1]));
	}
}


void cuda_sum(uint32_t *h_input, uint32_t *h_output ) {
	uint32_t *d_out, *d_in;

	cudaMalloc((uint32_t **)&d_out, N * sizeof(uint32_t));
	cudaMalloc((uint32_t **)&d_in, N * sizeof(uint32_t));
	cudaMemcpy(d_out, h_output, N * sizeof(uint32_t), cudaMemcpyHostToDevice);
	cudaMemcpy(d_in, h_input, N * sizeof(uint32_t), cudaMemcpyHostToDevice);

	// start timer
	cudaEvent_t start, stop;
	startTimer(start, stop);

	if (N <= ELEMENTS_PER_BLOCK) {
		cuda_sum_small(d_in, d_out, N);
	}
	else {

		cuda_sum_large(d_in, d_out, N);
	}

	cudaMemcpy(h_output, d_out, N * sizeof(uint32_t), cudaMemcpyDeviceToHost);
	// stop timer
	float elapsedTime = stopTimer(start, stop);
	cudaFree(d_out);
	cudaFree(d_in);
	printf("CUDA Implementation Time : %f\n", elapsedTime);
}

__host__ void thrust_sum(const uint32_t* input, uint32_t* output) {
  thrust::device_vector<uint32_t> d_in(input, input+N);
  thrust::device_vector<uint32_t> d_result(N);

  thrust::exclusive_scan(thrust::device, d_in.begin(), d_in.end(), d_result.begin());

  thrust::host_vector<uint32_t> h_result = d_result;

	for(int i =0; i<N; i++)
		output[i] = h_result[i];
}

int main(){
	uint32_t *in = new uint32_t[N];
	for (uint32_t i = 0; i < N; i++) {
		in[i] = rand() % N;
	}
	uint32_t *out = new uint32_t[N]();
	uint32_t *h_thrust_ref = new uint32_t[N];
	cudaEvent_t start, stop;
	startTimer(start, stop);
	thrust_sum(in, h_thrust_ref);
  	float elapsedTime = stopTimer(start, stop);
  	printf("Thrust Implementation : %f ms\n", elapsedTime);
	cuda_sum(in,out);
	// for(int i=0; i<N; i++)
	// 	printf("%d\t", out[i]);
	// printf("\n");
	check_result(h_thrust_ref, out, N);
}

//nvcc -O2 -std=c++14 -allow-unsupported-compiler -arch=sm_80 -lineinfo -res-usage -src-in-ptx problem2.cu -o prob2.out
