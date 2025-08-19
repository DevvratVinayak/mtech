#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <cuda.h>
#include <cuda_runtime.h>
#include <thrust/device_vector.h>
#include <thrust/host_vector.h>
#include <thrust/transform.h>
#include <thrust/execution_policy.h>
#include <vector>
#include <algorithm>

using namespace std;

#define NSEC_SEC_MUL (1.0e9)
#define MAX_SIZE 12000

// Structure to hold parameters for each grid search point
struct GridSearchParams {
    double *temp_q;       // Precomputed values for constraints
    double *mul_q;        // Multiplicative factors
    double *e;            // Constraints
    int *pnts;            // Pointer to result counter
    int *s;               // Size limits
    int* output;
};

// Functor to perform the grid search
struct GridSearch {
    GridSearchParams params;
    
  __device__
    void operator()(long long idx) {
        int r[10];
        long long temp = idx;
        for (int i = 9; i >= 0; --i) {
            r[i] = temp % 13;
            temp /= 13;
        }
        bool valid = true;
        for(int i = 0; i < 10; i++){
            double q = fabs(params.temp_q[i] + params.mul_q[i * 10 + 0] * r[0] +
                                       params.mul_q[i * 10 + 1] * r[1] +
                                       params.mul_q[i * 10 + 2] * r[2] +
                                       params.mul_q[i * 10 + 3] * r[3] +
                                       params.mul_q[i * 10 + 4] * r[4] +
                                       params.mul_q[i * 10 + 5] * r[5] +
                                       params.mul_q[i * 10 + 6] * r[6] +
                                       params.mul_q[i * 10 + 7] * r[7] +
                                       params.mul_q[i * 10 + 8] * r[8] +
                                       params.mul_q[i * 10 + 9] * r[9]);
            if(q > params.e[i]) {
                valid = false;
                break;
            }
        }
        if(valid) {
          atomicAdd(params.pnts, 1);
          int off = *params.pnts;
          // ri's which satisfy the constraints to be written in file
          for (int j = 0; j < 10; ++j) {
              params.output[off * 10 + j] = r[j];       

          }
        }
    }
};

void gridloopsearch(
    double dd1, double dd2, double dd3, double dd4, double dd5, double dd6, double dd7, double dd8,
    double dd9, double dd10, double dd11, double dd12, double dd13, double dd14, double dd15,
    double dd16, double dd17, double dd18, double dd19, double dd20, double dd21, double dd22,
    double dd23, double dd24, double dd25, double dd26, double dd27, double dd28, double dd29,
    double dd30, double c11, double c12, double c13, double c14, double c15, double c16, double c17,
    double c18, double c19, double c110, double d1, double ey1, double c21, double c22, double c23,
    double c24, double c25, double c26, double c27, double c28, double c29, double c210, double d2,
    double ey2, double c31, double c32, double c33, double c34, double c35, double c36, double c37,
    double c38, double c39, double c310, double d3, double ey3, double c41, double c42, double c43,
    double c44, double c45, double c46, double c47, double c48, double c49, double c410, double d4,
    double ey4, double c51, double c52, double c53, double c54, double c55, double c56, double c57,
    double c58, double c59, double c510, double d5, double ey5, double c61, double c62, double c63,
    double c64, double c65, double c66, double c67, double c68, double c69, double c610, double d6,
    double ey6, double c71, double c72, double c73, double c74, double c75, double c76, double c77,
    double c78, double c79, double c710, double d7, double ey7, double c81, double c82, double c83,
    double c84, double c85, double c86, double c87, double c88, double c89, double c810, double d8,
    double ey8, double c91, double c92, double c93, double c94, double c95, double c96, double c97,
    double c98, double c99, double c910, double d9, double ey9, double c101, double c102,
    double c103, double c104, double c105, double c106, double c107, double c108, double c109,
    double c1010, double d10, double ey10, double kk);

struct timespec begin_grid, end_main;

// to store values of disp.txt
double a[120];

// to store values of grid.txt
double b[30];

int main() {
  int i, j;

  i = 0;
  FILE* fp = fopen("./disp.txt", "r");
  if (fp == NULL) {
    printf("Error: could not open file\n");
    return 1;
  }

  while (!feof(fp)) {
    if (!fscanf(fp, "%lf", &a[i])) {
      printf("Error: fscanf failed while reading disp.txt\n");
      exit(EXIT_FAILURE);
    }
    i++;
  }
  fclose(fp);

  // read grid file
  j = 0;
  FILE* fpq = fopen("./grid.txt", "r");
  if (fpq == NULL) {
    printf("Error: could not open file\n");
    return 1;
  }

  while (!feof(fpq)) {
    if (!fscanf(fpq, "%lf", &b[j])) {
      printf("Error: fscanf failed while reading grid.txt\n");
      exit(EXIT_FAILURE);
    }
    j++;
  }
  fclose(fpq);

  // grid value initialize
  // initialize value of kk;
  double kk = 0.3;

//   clock_gettime(CLOCK_MONOTONIC_RAW, &begin_grid);
  gridloopsearch(b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12],
                 b[13], b[14], b[15], b[16], b[17], b[18], b[19], b[20], b[21], b[22], b[23], b[24],
                 b[25], b[26], b[27], b[28], b[29], a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7],
                 a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15], a[16], a[17], a[18], a[19],
                 a[20], a[21], a[22], a[23], a[24], a[25], a[26], a[27], a[28], a[29], a[30], a[31],
                 a[32], a[33], a[34], a[35], a[36], a[37], a[38], a[39], a[40], a[41], a[42], a[43],
                 a[44], a[45], a[46], a[47], a[48], a[49], a[50], a[51], a[52], a[53], a[54], a[55],
                 a[56], a[57], a[58], a[59], a[60], a[61], a[62], a[63], a[64], a[65], a[66], a[67],
                 a[68], a[69], a[70], a[71], a[72], a[73], a[74], a[75], a[76], a[77], a[78], a[79],
                 a[80], a[81], a[82], a[83], a[84], a[85], a[86], a[87], a[88], a[89], a[90], a[91],
                 a[92], a[93], a[94], a[95], a[96], a[97], a[98], a[99], a[100], a[101], a[102],
                 a[103], a[104], a[105], a[106], a[107], a[108], a[109], a[110], a[111], a[112],
                 a[113], a[114], a[115], a[116], a[117], a[118], a[119], kk);
//   clock_gettime(CLOCK_MONOTONIC_RAW, &end_main);
//   printf("Total time = %f seconds\n", (end_main.tv_nsec - begin_grid.tv_nsec) / NSEC_SEC_MUL +
//                                           (end_main.tv_sec - begin_grid.tv_sec));

  return EXIT_SUCCESS;
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

// grid search function with loop variables

void gridloopsearch(
    double dd1, double dd2, double dd3, double dd4, double dd5, double dd6, double dd7, double dd8,
    double dd9, double dd10, double dd11, double dd12, double dd13, double dd14, double dd15,
    double dd16, double dd17, double dd18, double dd19, double dd20, double dd21, double dd22,
    double dd23, double dd24, double dd25, double dd26, double dd27, double dd28, double dd29,
    double dd30, double c11, double c12, double c13, double c14, double c15, double c16, double c17,
    double c18, double c19, double c110, double d1, double ey1, double c21, double c22, double c23,
    double c24, double c25, double c26, double c27, double c28, double c29, double c210, double d2,
    double ey2, double c31, double c32, double c33, double c34, double c35, double c36, double c37,
    double c38, double c39, double c310, double d3, double ey3, double c41, double c42, double c43,
    double c44, double c45, double c46, double c47, double c48, double c49, double c410, double d4,
    double ey4, double c51, double c52, double c53, double c54, double c55, double c56, double c57,
    double c58, double c59, double c510, double d5, double ey5, double c61, double c62, double c63,
    double c64, double c65, double c66, double c67, double c68, double c69, double c610, double d6,
    double ey6, double c71, double c72, double c73, double c74, double c75, double c76, double c77,
    double c78, double c79, double c710, double d7, double ey7, double c81, double c82, double c83,
    double c84, double c85, double c86, double c87, double c88, double c89, double c810, double d8,
    double ey8, double c91, double c92, double c93, double c94, double c95, double c96, double c97,
    double c98, double c99, double c910, double d9, double ey9, double c101, double c102,
    double c103, double c104, double c105, double c106, double c107, double c108, double c109,
    double c1010, double d10, double ey10, double kk) {

  double ey[10] = {ey1, ey2, ey3, ey4, ey5, ey6, ey7, ey8, ey9, ey10};
  // results points
  int pnts[] = {0};
  thrust::device_vector<int> d_pnts(pnts, pnts+1);

  double dd_values[] = {dd1,dd4,dd7,dd10,dd13,dd16,dd19,dd22, dd25,dd28};

  double dd_values1[]  = {dd3,dd6,dd9,dd12,dd15,dd18,dd21,dd24,dd27,dd30};

  // opening the "results-v0.txt" for writing he results in append mode
  FILE* fptr = fopen("./results-v01.txt", "w");
  if (fptr == NULL) {
    printf("Error in creating file !");
    exit(1);
  }
   thrust::device_vector<double> e;
  for (int i = 0; i < 10; i++) {
    e.push_back(kk * ey[i]);
  }

  int s[10];
    s[0] = floor((dd2 - dd1) / dd3);
  s[1] = floor((dd5 - dd4) / dd6);
  s[2] = floor((dd8 - dd7) / dd9);
  s[3] = floor((dd11 - dd10) / dd12);
  s[4] = floor((dd14 - dd13) / dd15);
  s[5] = floor((dd17 - dd16) / dd18);
  s[6] = floor((dd20 - dd19) / dd21);
  s[7] = floor((dd23 - dd22) / dd24);
  s[8] = floor((dd26 - dd25) / dd27);
  s[9] = floor((dd29 - dd28) / dd30);

   
  thrust::device_vector<int> d_s(s, s+10); 
  //Computing loop invariant additions and multiplications to avoid recomputations during loops

  double temp_q[] = {c11*dd1+c12*dd4+c13*dd7+c14*dd10+c15*dd13+c16*dd16+c17*dd19+c18*dd22+c19*dd25+c110*dd28-d1,
                    c21*dd1+c22*dd4+c23*dd7+c24*dd10+c25*dd13+c26*dd16+c27*dd19+c28*dd22+c29*dd25+c210*dd28-d2,
                    c31*dd1+c32*dd4+c33*dd7+c34*dd10+c35*dd13+c36*dd16+c37*dd19+c38*dd22+c39*dd25+c310*dd28-d3,
                    c41*dd1+c42*dd4+c43*dd7+c44*dd10+c45*dd13+c46*dd16+c47*dd19+c48*dd22+c49*dd25+c410*dd28-d4,
                    c51*dd1+c52*dd4+c53*dd7+c54*dd10+c55*dd13+c56*dd16+c57*dd19+c58*dd22+c59*dd25+c510*dd28-d5,
                    c61*dd1+c62*dd4+c63*dd7+c64*dd10+c65*dd13+c66*dd16+c67*dd19+c68*dd22+c69*dd25+c610*dd28-d6,
                    c71*dd1+c72*dd4+c73*dd7+c74*dd10+c75*dd13+c76*dd16+c77*dd19+c78*dd22+c79*dd25+c710*dd28-d7,
                    c81*dd1+c82*dd4+c83*dd7+c84*dd10+c85*dd13+c86*dd16+c87*dd19+c88*dd22+c89*dd25+c810*dd28-d8,
                    c91*dd1+c92*dd4+c93*dd7+c94*dd10+c95*dd13+c96*dd16+c97*dd19+c98*dd22+c99*dd25+c910*dd28-d9,
                    c101*dd1+c102*dd4+c103*dd7+c104*dd10+c105*dd13+c106*dd16+c107*dd19+c108*dd22+c109*dd25+c1010*dd28-d10

                                };
  thrust::device_vector<double> d_temp_q(temp_q, temp_q+10); 

    //Computing index multipliers and storing them in an array to avoid recomputations during loops
    double mul_q[] = {c11*dd3, c12*dd6, c13*dd9, c14*dd12, c15*dd15, c16*dd18, c17*dd21, c18*dd24, c19*dd27, c110*dd30,
          c21*dd3, c22*dd6, c23*dd9, c24*dd12, c25*dd15, c26*dd18, c27*dd21, c28*dd24, c29*dd27, c210*dd30,
          c31*dd3, c32*dd6, c33*dd9, c34*dd12, c35*dd15, c36*dd18, c37*dd21, c38*dd24, c39*dd27, c310*dd30,
          c41*dd3, c42*dd6, c43*dd9, c44*dd12, c45*dd15, c46*dd18, c47*dd21, c48*dd24, c49*dd27, c410*dd30,
          c51*dd3, c52*dd6, c53*dd9, c54*dd12, c55*dd15, c16*dd18, c57*dd21, c58*dd24, c59*dd27, c510*dd30,
          c61*dd3, c62*dd6, c63*dd9, c64*dd12, c65*dd15, c66*dd18, c67*dd21, c68*dd24, c69*dd27, c610*dd30,
          c71*dd3, c72*dd6, c73*dd9, c74*dd12, c75*dd15, c76*dd18, c77*dd21, c78*dd24, c79*dd27, c710*dd30,
          c81*dd3, c82*dd6, c83*dd9, c84*dd12, c85*dd15, c86*dd18, c87*dd21, c88*dd24, c89*dd27, c810*dd30,
          c91*dd3, c92*dd6, c93*dd9, c94*dd12, c95*dd15, c96*dd18, c97*dd21, c98*dd24, c99*dd27, c910*dd30,
          c101*dd3, c102*dd6, c103*dd9, c104*dd12, c105*dd15, c106*dd18, c107*dd21, c108*dd24, c109*dd27, c1010*dd30};
    
    thrust::device_vector<double> d_mul_q(mul_q, mul_q+100);
    thrust::host_vector<int> h_output(MAX_SIZE * 10);
    thrust::device_vector<int> output(MAX_SIZE * 10);

    GridSearchParams params;
    params.temp_q = thrust::raw_pointer_cast(d_temp_q.data());
    params.mul_q = thrust::raw_pointer_cast(d_mul_q.data());
    params.e = thrust::raw_pointer_cast(e.data());
    params.pnts = thrust::raw_pointer_cast(d_pnts.data());
    params.s = thrust::raw_pointer_cast(d_s.data());
    params.output = thrust::raw_pointer_cast(output.data());

    long long total_size =  1;
    for(int i=0; i<10; i++)
        total_size *= s[i];
    // printf("total_sizee : %lld ", total_size);
    thrust::counting_iterator<long long> begin(0);
    thrust::counting_iterator<long long> end(total_size);
    
    // Create the grid search functor
    GridSearch gridSearch = {params};
    
    // Run the parallel computation
    cudaEvent_t start, stop;
    startTimer(start, stop);
    thrust::for_each(thrust::device, begin, end, gridSearch);
    thrust::copy(d_pnts.begin(), d_pnts.end(), pnts);
    float kernel_time = stopTimer(start, stop);
    printf("Kernel Time  : %f  seconds \n", kernel_time/1000);


    thrust::copy(output.begin(), output.end(), h_output.begin());


      double x;
      for(int i=1; i<=pnts[0]; i++){
        for(int j=0; j<9; j++){
          x = dd_values[j] +  h_output[i*10+j] * dd_values1[j];
          fprintf(fptr, "%lf\t", x);
        }
        x = dd_values[9] +  h_output[i*10+9] * dd_values1[9];
        fprintf(fptr, "%lf\n", x);
      }
    std::cout << "result pnts: " << d_pnts[0] << std::endl;
    fclose(fptr);

  // end function gridloopsearch
}

// nvcc -O3 -std=c++17 -allow-unsupported-compiler -arch=sm_80 -lineinfo -res-usage -src-in-ptx problem3-thrust.cu -o prob3.out
// ./prob3.out