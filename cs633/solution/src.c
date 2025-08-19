#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <stdbool.h>

#define MAX_N 1024
#define MAX_T 1000

int rank, size;
int NX, NY, NZ, NC;         // Grid dimensions & time steps
int PX, PY, PZ;             // Process grid dimensions
int sub_nx, sub_ny, sub_nz; // Local subdomain size

int start_x, start_y, start_z; // Start indices of subdomain
int neighbors[6];              // Left, Right, Up, Down, Front, Back

// Function prototypes

float ***allocate_3d_array(int nx, int ny, int nz);
void free_3d_array(float ***array);
void compute_decomposition();
void exchange_boundary_data(float ***local_data);
void find_minima_maxima(float ***local_data, int *local_minima_count, int *local_maxima_count, float *local_min, float *local_max);
void write_output(const char *filename, int *minima_counts, int *maxima_counts, float *global_mins, float *global_maxs, double read_time, double main_time, double total_time);

int main(int argc, char **argv)
{
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 10)
    {
        if (rank == 0)
            printf("Example Usage: mpirun -np 64 ./a.out <inputfile> <PX> <PY> <PZ> <NX> <NY> <NZ> <NC> <outfile>\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
        return -1;
    }

    char *filename = argv[1];
    PX = atoi(argv[2]);
    PY = atoi(argv[3]);
    PZ = atoi(argv[4]);
    NX = atoi(argv[5]);
    NY = atoi(argv[6]);
    NZ = atoi(argv[7]);
    NC = atoi(argv[8]);
    char *outfile = argv[9];

    if (PX * PY * PZ != size)
    {
        if (rank == 0)
            printf("Error: P must equal number of processes!\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
        return -1;
    }

    if (NX % PX != 0 || NY % PY != 0 || NZ % PZ != 0)
    {
        if (rank == 0)
            printf("Error: We are asked to assume NX, NY and NZ are divisible by PX,PY and PZ respectively\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
        return -1;
    }

    if (!(PX >= 1 && PY >= 1 && PZ >= 1 &&
          NX <= 1024 && NY <= 1024 && NZ <= 1024 &&
          NC >= 1 && NC <= 1000))
    {
        if (rank == 0)
            printf("Error: Constraints did not match!\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
        return -1;
    }

    compute_decomposition();

    double time_1 = MPI_Wtime();

    ////////////// new code
    // Parallel MPI I/O: Each process reads its own part
    MPI_File fh;
    MPI_Status status;

    // Open file
    if (MPI_File_open(MPI_COMM_WORLD, filename, MPI_MODE_RDONLY, MPI_INFO_NULL, &fh) != MPI_SUCCESS)
    {
        printf("Error opening file for MPI I/O - rank %d\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    MPI_Datatype floatNC_type;
    MPI_Type_contiguous(NC, MPI_FLOAT, &floatNC_type);
    MPI_Type_commit(&floatNC_type);

    // Now create a subarray of (NZ, NY, NX) blocks
    int sizes[3] = {NZ, NY, NX};                 // Full global domain
    int subsizes[3] = {sub_nz, sub_ny, sub_nx};  // Local domain
    int starts[3] = {start_z, start_y, start_x}; // Starting point for this process

    MPI_Datatype filetype;
    MPI_Type_create_subarray(3, sizes, subsizes, starts, MPI_ORDER_C, floatNC_type, &filetype);
    MPI_Type_commit(&filetype);

    // Set file view with floatNC_type
    MPI_File_set_view(fh, 0, MPI_FLOAT, filetype, "native", MPI_INFO_NULL);

    // Allocate local buffer
    int local_size = sub_nx * sub_ny * sub_nz * NC;
    float *local_buffer = (float *)malloc(local_size * sizeof(float));
    if (!local_buffer)
    {
        printf("Failed to allocate local read buffer\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    // Read all timesteps
    MPI_File_read_all(fh, local_buffer, local_size, MPI_FLOAT, &status);

    // Done reading
    MPI_Type_free(&filetype);
    MPI_File_close(&fh);
    //////////////new code ends

    double time_2 = MPI_Wtime();

    // Arrays to store results for each time step
    int *minima_counts = (int *)malloc(NC * sizeof(int));
    int *maxima_counts = (int *)malloc(NC * sizeof(int));
    float *global_mins = (float *)malloc(NC * sizeof(float));
    float *global_maxs = (float *)malloc(NC * sizeof(float));

    if (!minima_counts || !maxima_counts || !global_mins || !global_maxs)
    {
        printf("Failed to allocate result arrays\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
        return -1;
    }

    ///// new code loop
    for (int t = 0; t < NC; t++)
    {
        // Allocate local_data with ghost cells
        float ***local_data = allocate_3d_array(sub_nx, sub_ny, sub_nz);

        // Fill local_data from local_buffer for this timestep
        for (int x = 1; x <= sub_nx; x++)
        {
            for (int y = 1; y <= sub_ny; y++)
            {
                for (int z = 1; z <= sub_nz; z++)
                {
                    int flat_index = (((z - 1) * sub_ny + (y - 1)) * sub_nx + (x - 1)) * NC + t;
                    local_data[x][y][z] = local_buffer[flat_index];
                }
            }
        }

        // Exchange boundaries
        exchange_boundary_data(local_data);

        // Local minima/maxima
        int local_minima_count = 0, local_maxima_count = 0;
        float local_min = FLT_MAX, local_max = -FLT_MAX;
        find_minima_maxima(local_data, &local_minima_count, &local_maxima_count, &local_min, &local_max);

        // Global reductions
        int global_minima_count, global_maxima_count;
        float global_min, global_max;
        MPI_Reduce(&local_minima_count, &global_minima_count, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(&local_maxima_count, &global_maxima_count, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        MPI_Reduce(&local_min, &global_min, 1, MPI_FLOAT, MPI_MIN, 0, MPI_COMM_WORLD);
        MPI_Reduce(&local_max, &global_max, 1, MPI_FLOAT, MPI_MAX, 0, MPI_COMM_WORLD);

        if (rank == 0)
        {
            minima_counts[t] = global_minima_count;
            maxima_counts[t] = global_maxima_count;
            global_mins[t] = global_min;
            global_maxs[t] = global_max;
        }

        free_3d_array(local_data); // Free local array after processing
    }
    free(local_buffer);

    /////new code loop ends

    double time_3 = MPI_Wtime();

    double read_time = time_2 - time_1;
    double main_time = time_3 - time_2;
    double total_time = time_3 - time_1;

    // Get maximum times across all processes
    double max_read_time, max_main_time, max_total_time;
    MPI_Reduce(&read_time, &max_read_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&main_time, &max_main_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&total_time, &max_total_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    // Write output file from rank 0
    if (rank == 0)
    {
        write_output(outfile, minima_counts, maxima_counts, global_mins, global_maxs,
                     max_read_time, max_main_time, max_total_time);
    }

    // Free result arrays
    free(minima_counts);
    free(maxima_counts);
    free(global_mins);
    free(global_maxs);
    MPI_Type_free(&floatNC_type);
    // MPI_Type_free(&filetype);

    MPI_Finalize();
    return 0;
}

// Function to compute 3D decomposition
void compute_decomposition()
{
    sub_nx = NX / PX;
    sub_ny = NY / PY;
    sub_nz = NZ / PZ;

    int px = rank % PX;
    int py = (rank / PX) % PY;
    int pz = rank / (PX * PY);

    start_x = px * sub_nx;
    start_y = py * sub_ny;
    start_z = pz * sub_nz;

    neighbors[0] = (px == 0) ? MPI_PROC_NULL : rank - 1;              // Left
    neighbors[1] = (px == PX - 1) ? MPI_PROC_NULL : rank + 1;         // Right
    neighbors[2] = (py == 0) ? MPI_PROC_NULL : rank - PX;             // Down
    neighbors[3] = (py == PY - 1) ? MPI_PROC_NULL : rank + PX;        // Up
    neighbors[4] = (pz == 0) ? MPI_PROC_NULL : rank - (PX * PY);      // Front
    neighbors[5] = (pz == PZ - 1) ? MPI_PROC_NULL : rank + (PX * PY); // Back
}

// Function to allocate memory for a 3D array
float ***allocate_3d_array(int nx, int ny, int nz)
{
    int padded_nx = nx + 2;
    int padded_ny = ny + 2;
    int padded_nz = nz + 2;

    float ***array = (float ***)malloc(padded_nx * sizeof(float **));
    float **plane = (float **)malloc(padded_nx * padded_ny * sizeof(float *));
    float *data = (float *)calloc((size_t)padded_nx * padded_ny * padded_nz, sizeof(float));

    if (!array || !plane || !data)
    {
        printf("Rank %d: Allocation failed\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    for (int x = 0; x < padded_nx; x++)
    {
        array[x] = &plane[x * padded_ny];
        for (int y = 0; y < padded_ny; y++)
        {
            array[x][y] = &data[(x * padded_ny + y) * padded_nz];
        }
    }

    return array;
}

// Function to free memory of a 3D array
void free_3d_array(float ***array)
{
    if (!array)
        return;
    if (array[0])
    {
        if (array[0][0])
            free(array[0][0]); // free data
        free(array[0]);        // free plane
    }
    free(array); // free array
}

// Function to exchange boundary data with neighboring processes
void exchange_boundary_data(float ***local_data)
{
    MPI_Status status;

    // Allocate buffers for sending and receiving boundary layers
    float *send_left = (float *)malloc(sub_ny * sub_nz * sizeof(float));
    float *send_right = (float *)malloc(sub_ny * sub_nz * sizeof(float));
    float *recv_left = (float *)malloc(sub_ny * sub_nz * sizeof(float));
    float *recv_right = (float *)malloc(sub_ny * sub_nz * sizeof(float));

    float *send_down = (float *)malloc(sub_nx * sub_nz * sizeof(float));
    float *send_up = (float *)malloc(sub_nx * sub_nz * sizeof(float));
    float *recv_down = (float *)malloc(sub_nx * sub_nz * sizeof(float));
    float *recv_up = (float *)malloc(sub_nx * sub_nz * sizeof(float));

    float *send_front = (float *)malloc(sub_nx * sub_ny * sizeof(float));
    float *send_back = (float *)malloc(sub_nx * sub_ny * sizeof(float));
    float *recv_front = (float *)malloc(sub_nx * sub_ny * sizeof(float));
    float *recv_back = (float *)malloc(sub_nx * sub_ny * sizeof(float));

    // Check if allocations succeeded
    if (!send_left || !send_right || !recv_left || !recv_right ||
        !send_down || !send_up || !recv_down || !recv_up ||
        !send_front || !send_back || !recv_front || !recv_back)
    {
        printf("Rank %d: Failed to allocate boundary exchange buffers\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 1);
        return;
    }

    // Pack boundary data for sending

    // Left and right faces (YZ planes)
    for (int y = 1; y <= sub_ny; y++)
    {
        for (int z = 1; z <= sub_nz; z++)
        {
            send_left[(y - 1) * sub_nz + (z - 1)] = local_data[1][y][z];       // First real X layer
            send_right[(y - 1) * sub_nz + (z - 1)] = local_data[sub_nx][y][z]; // Last real X layer
        }
    }

    // Down and up faces (XZ planes)
    for (int x = 1; x <= sub_nx; x++)
    {
        for (int z = 1; z <= sub_nz; z++)
        {
            send_down[(x - 1) * sub_nz + (z - 1)] = local_data[x][1][z];    // First real Y layer
            send_up[(x - 1) * sub_nz + (z - 1)] = local_data[x][sub_ny][z]; // Last real Y layer
        }
    }

    // Front and back faces (XY planes)
    for (int x = 1; x <= sub_nx; x++)
    {
        for (int y = 1; y <= sub_ny; y++)
        {
            send_front[(x - 1) * sub_ny + (y - 1)] = local_data[x][y][1];     // First real Z layer
            send_back[(x - 1) * sub_ny + (y - 1)] = local_data[x][y][sub_nz]; // Last real Z layer
        }
    }

    // Exchange X-direction boundary data
    MPI_Sendrecv(send_left, sub_ny * sub_nz, MPI_FLOAT, neighbors[0], 0,
                 recv_right, sub_ny * sub_nz, MPI_FLOAT, neighbors[1], 0,
                 MPI_COMM_WORLD, &status);

    MPI_Sendrecv(send_right, sub_ny * sub_nz, MPI_FLOAT, neighbors[1], 1,
                 recv_left, sub_ny * sub_nz, MPI_FLOAT, neighbors[0], 1,
                 MPI_COMM_WORLD, &status);

    // After X-direction exchange:
    for (int y = 1; y <= sub_ny; y++)
    {
        for (int z = 1; z <= sub_nz; z++)
        {
            if (neighbors[0] != MPI_PROC_NULL)
                local_data[0][y][z] = recv_left[(y - 1) * sub_nz + (z - 1)];
            if (neighbors[1] != MPI_PROC_NULL)
                local_data[sub_nx + 1][y][z] = recv_right[(y - 1) * sub_nz + (z - 1)];
        }
    }

    // Exchange Y-direction boundary data
    MPI_Sendrecv(send_down, sub_nx * sub_nz, MPI_FLOAT, neighbors[2], 2,
                 recv_up, sub_nx * sub_nz, MPI_FLOAT, neighbors[3], 2,
                 MPI_COMM_WORLD, &status);

    MPI_Sendrecv(send_up, sub_nx * sub_nz, MPI_FLOAT, neighbors[3], 3,
                 recv_down, sub_nx * sub_nz, MPI_FLOAT, neighbors[2], 3,
                 MPI_COMM_WORLD, &status);

    // After Y-direction exchange:
    for (int x = 1; x <= sub_nx; x++)
    {
        for (int z = 1; z <= sub_nz; z++)
        {
            if (neighbors[2] != MPI_PROC_NULL)
                local_data[x][0][z] = recv_down[(x - 1) * sub_nz + (z - 1)];
            if (neighbors[3] != MPI_PROC_NULL)
                local_data[x][sub_ny + 1][z] = recv_up[(x - 1) * sub_nz + (z - 1)];
        }
    }

    // Exchange Z-direction boundary data
    MPI_Sendrecv(send_front, sub_nx * sub_ny, MPI_FLOAT, neighbors[4], 4,
                 recv_back, sub_nx * sub_ny, MPI_FLOAT, neighbors[5], 4,
                 MPI_COMM_WORLD, &status);

    MPI_Sendrecv(send_back, sub_nx * sub_ny, MPI_FLOAT, neighbors[5], 5,
                 recv_front, sub_nx * sub_ny, MPI_FLOAT, neighbors[4], 5,
                 MPI_COMM_WORLD, &status);

    // After Z-direction exchange:
    for (int x = 1; x <= sub_nx; x++)
    {
        for (int y = 1; y <= sub_ny; y++)
        {
            if (neighbors[4] != MPI_PROC_NULL)
                local_data[x][y][0] = recv_front[(x - 1) * sub_ny + (y - 1)];
            if (neighbors[5] != MPI_PROC_NULL)
                local_data[x][y][sub_nz + 1] = recv_back[(x - 1) * sub_ny + (y - 1)];
        }
    }

    // Clean up boundary exchange buffers
    free(send_left);
    free(send_right);
    free(recv_left);
    free(recv_right);
    free(send_down);
    free(send_up);
    free(recv_down);
    free(recv_up);
    free(send_front);
    free(send_back);
    free(recv_front);
    free(recv_back);
}

// Function to find local minima, maxima and local min/max values
void find_minima_maxima(float ***local_data, int *local_minima_count, int *local_maxima_count,
                        float *local_min, float *local_max)
{
    // The 6 directions: left, right, down, up, front, back
    const int dx[] = {-1, 1, 0, 0, 0, 0};
    const int dy[] = {0, 0, -1, 1, 0, 0};
    const int dz[] = {0, 0, 0, 0, -1, 1};

    // Find local minimum and maximum values
    for (int x = 1; x <= sub_nx; x++)
    {
        for (int y = 1; y <= sub_ny; y++)
        {
            for (int z = 1; z <= sub_nz; z++)
            {
                float val = local_data[x][y][z];

                // Update local min and max
                if (val < *local_min)
                    *local_min = val;
                if (val > *local_max)
                    *local_max = val;

                // Check if this point is a local minimum or maximum
                bool is_min = true;
                bool is_max = true;

                // Check all 6 face-adjacent neighbors
                for (int i = 0; i < 6; i++)
                {
                    // Skip if this is a boundary and there's no neighbor in this direction
                    if ((x == 1 && dx[i] == -1 && neighbors[0] == MPI_PROC_NULL) ||
                        (x == sub_nx && dx[i] == 1 && neighbors[1] == MPI_PROC_NULL) ||
                        (y == 1 && dy[i] == -1 && neighbors[2] == MPI_PROC_NULL) ||
                        (y == sub_ny && dy[i] == 1 && neighbors[3] == MPI_PROC_NULL) ||
                        (z == 1 && dz[i] == -1 && neighbors[4] == MPI_PROC_NULL) ||
                        (z == sub_nz && dz[i] == 1 && neighbors[5] == MPI_PROC_NULL))
                    {
                        continue;
                    }

                    float neighbor_val = local_data[x + dx[i]][y + dy[i]][z + dz[i]];

                    // If any neighbor is smaller or equal, not a min
                    if (neighbor_val <= val)
                        is_min = false;

                    // If any neighbor is larger or equal, not a max
                    if (neighbor_val >= val)
                        is_max = false;

                    // Early exit if neither min nor max
                    if (!is_min && !is_max)
                        break;
                }

                // Only count if we checked all available neighbors
                if (is_min)
                    (*local_minima_count)++;
                if (is_max)
                    (*local_maxima_count)++;
            }
        }
    }
}

// Function to write output to file
void write_output(const char *filename, int *minima_counts, int *maxima_counts,
                  float *global_mins, float *global_maxs,
                  double read_time, double main_time, double total_time)
{
    FILE *fp = fopen(filename, "w");
    if (!fp)
    {
        printf("Error opening output file: %s\n", filename);
        return;
    }

    // Line 1: (count of local minima, count of local maxima), ... for each time step
    for (int t = 0; t < NC; t++)
    {
        fprintf(fp, "(%d, %d)", minima_counts[t], maxima_counts[t]);
        if (t < NC - 1)
            fprintf(fp, ", ");
    }
    fprintf(fp, "\n");

    // Line 2: (global minimum, global maximum), ... for each time step
    for (int t = 0; t < NC; t++)
    {
        fprintf(fp, "(%.4f, %.4f)", global_mins[t], global_maxs[t]);
        if (t < NC - 1)
            fprintf(fp, ", ");
    }
    fprintf(fp, "\n");

    // Line 3: Read Time, Main code Time, Total time
    fprintf(fp, "%lf, %lf, %lf\n", read_time, main_time, total_time);

    fclose(fp);
}