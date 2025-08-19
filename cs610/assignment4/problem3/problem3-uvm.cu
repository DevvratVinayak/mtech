#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <cuda.h>
#include <cuda_runtime.h>
#include <vector>
#include <algorithm>

#define NSEC_SEC_MUL (1.0e9)
#define MAX_SIZE 12000

using namespace std;


vector<vector<int>> arrayToVector(int *arr, int rows, int cols) {
    vector<vector<int>> vec(rows, vector<int>(cols)); // Create a 2D vector with the same dimensions

    // Copy the elements from the array to the vector
    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            vec[i][j] = arr[i* cols + j];
            // printf("%d\t", vec[i][j]);
        }
        // printf("\n");
    }

    return vec; // Return the populated vector
}

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


__global__ void cuda_search(int *s, double *mul_q, double *temp_q, double *e, int* pnts, int *d_output){
  double q[10] = {0};
  int r1 = blockIdx.x * blockDim.x + threadIdx.x;
  int r2 = blockIdx.y * blockDim.y + threadIdx.y;
  int r3 = blockIdx.z * blockDim.z + threadIdx.z;

  if(r1 >= s[0] || r2 >= s[1] || r3 >= s[2] ) return;
  // grid search starts
    for (int r4 = 0; r4 < s[3]; ++r4) {

      for (int r5 = 0; r5 < s[4]; ++r5) {

        for (int r6 = 0; r6 < s[5]; ++r6) {

          for (int r7 = 0; r7 < s[6]; ++r7) {

            for (int r8 = 0; r8 < s[7]; ++r8) {

              for (int r9 = 0; r9 < s[8]; ++r9) {

                for (int r10 = 0; r10 < s[9]; ++r10) {

                  // constraints
                  bool valid = true;
                  for(int i=0; i<10; i++){
                    q[i] = fabs(temp_q[i]+mul_q[i * 10 + 0]*r1+mul_q[i * 10 + 1]*r2+mul_q[i * 10 + 2]*r3+mul_q[i * 10 + 3]*r4+
                            mul_q[i * 10 + 4]*r5+mul_q[i * 10 + 5]*r6+mul_q[i * 10 + 6]*r7+mul_q[i * 10 + 7]*r8+
                            mul_q[i * 10 + 8]*r9+mul_q[i * 10 + 9]*r10);
                    if(q[i] > e[i]){
                      valid = false;
                      break;
                    } 
                  }
                  if(valid){
                  int r[] = {r1,r2,r3,r4,r5,r6,r7,r8,r9,r10};
                    atomicAdd(pnts, 1);
                    int off = *pnts;
                    // ri's which satisfy the constraints to be written in file
                     for (int j = 0; j < 10; ++j) {
                          d_output[off * 10 + j] = r[j];       

                      }
                    
                    }

                }
              }
            }
          }
        }
      }
    }

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

  double* e;
  cudaMallocManaged(&e, 10 * sizeof(double));
  // results points
  int *pnts;
  cudaMallocManaged(&pnts, sizeof(int));
  pnts[0] = 0;

  double ey[10] = {ey1, ey2, ey3, ey4, ey5, ey6, ey7, ey8, ey9, ey10};

  double dd_values[10] = {dd1, dd4, dd7, dd10, dd13, dd16, dd19, dd22, dd25, dd28};
  double dd_values1[10] = {dd3, dd6, dd9, dd12, dd15, dd18, dd21, dd24, dd27, dd30};


  // opening the "results-v0.txt" for writing he results in append mode
  FILE* fptr = fopen("./results-v0-1.txt", "w");
  if (fptr == NULL) {
    printf("Error in creating file !");
    exit(1);
  }

  for (int i = 0; i < 10; i++) {
    e[i] = kk * ey[i];
  }

  // for loop upper values
  int* s;
  cudaMallocManaged(&s, 10 * sizeof(int));
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

  double* temp_q;
  cudaMallocManaged(&temp_q, 10 * sizeof(double));

  int* output;
  cudaMallocManaged(&output, MAX_SIZE * 10 * sizeof(int));

  //Computing loop invariant additions and multiplications to avoid recomputations during loops
  temp_q[0]  = c11*dd1+c12*dd4+c13*dd7+c14*dd10+c15*dd13+c16*dd16+c17*dd19+c18*dd22+c19*dd25+c110*dd28-d1;
  temp_q[1]  = c21*dd1+c22*dd4+c23*dd7+c24*dd10+c25*dd13+c26*dd16+c27*dd19+c28*dd22+c29*dd25+c210*dd28-d2;
  temp_q[2] =  c31*dd1+c32*dd4+c33*dd7+c34*dd10+c35*dd13+c36*dd16+c37*dd19+c38*dd22+c39*dd25+c310*dd28-d3;
  temp_q[3] =  c41*dd1+c42*dd4+c43*dd7+c44*dd10+c45*dd13+c46*dd16+c47*dd19+c48*dd22+c49*dd25+c410*dd28-d4;
  temp_q[4] =  c51*dd1+c52*dd4+c53*dd7+c54*dd10+c55*dd13+c56*dd16+c57*dd19+c58*dd22+c59*dd25+c510*dd28-d5;
  temp_q[5]  = c61*dd1+c62*dd4+c63*dd7+c64*dd10+c65*dd13+c66*dd16+c67*dd19+c68*dd22+c69*dd25+c610*dd28-d6;
  temp_q[6]  = c71*dd1+c72*dd4+c73*dd7+c74*dd10+c75*dd13+c76*dd16+c77*dd19+c78*dd22+c79*dd25+c710*dd28-d7;
  temp_q[7] =  c81*dd1+c82*dd4+c83*dd7+c84*dd10+c85*dd13+c86*dd16+c87*dd19+c88*dd22+c89*dd25+c810*dd28-d8;
  temp_q[8] =  c91*dd1+c92*dd4+c93*dd7+c94*dd10+c95*dd13+c96*dd16+c97*dd19+c98*dd22+c99*dd25+c910*dd28-d9;
  temp_q[9] =  c101*dd1+c102*dd4+c103*dd7+c104*dd10+c105*dd13+c106*dd16+c107*dd19+c108*dd22+c109*dd25+c1010*dd28-d10;

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

      
      double *d_mul_q;
      cudaMalloc(&d_mul_q, 100 * sizeof(double));
      
      cudaMemcpy(d_mul_q, mul_q, 100 * sizeof(double), cudaMemcpyHostToDevice);
      
      cudaEvent_t start, stop;
      dim3 block(4, 4, 4);
      dim3 grid((s[0] + block.x - 1) / block.x, 
            (s[1] + block.y - 1) / block.y, 
            (s[2] + block.z - 1) / block.z);
      startTimer(start, stop);
      cuda_search<<<grid, block>>>(s, d_mul_q,temp_q, e, pnts, output);
      cudaDeviceSynchronize();
      float kernel_time = stopTimer(start, stop);
      printf("Kernel Time  : %f  seconds \n", kernel_time/1000);
       vector<vector<int>> indices_vector = arrayToVector(output, *pnts+1, 10);
       sort(indices_vector.begin()+1, indices_vector.end(), [](const vector<int>& a, vector<int>& b) {
          return a[0] < b[0];
      });
      double x;
      for(int i=1; i<=pnts[0]; i++){
        for(int j=0; j<9; j++){
          x = dd_values[j] +  indices_vector[i][j] * dd_values1[j];
          fprintf(fptr, "%lf\t", x);
        }
        x = dd_values[9] +  indices_vector[i][9] * dd_values1[9];
        fprintf(fptr, "%lf\n", x);
      }

      fclose(fptr);
      printf("result pnts: %d\n", pnts[0]);
      
      
      cudaFree(e);
      cudaFree(d_mul_q);
      cudaFree(temp_q);
      cudaFree(s);
      cudaFree(pnts);
      cudaFree(output);


  // end function gridloopsearch
}

//nvcc -O3 -std=c++17 -allow-unsupported-compiler -arch=sm_80 -lineinfo -res-usage -src-in-ptx problem3-uvm.cu -o prob3.out
//./prob3.out