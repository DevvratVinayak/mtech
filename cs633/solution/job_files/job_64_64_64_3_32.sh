#!/bin/bash
#SBATCH -N 1
#SBATCH --ntasks-per-node=32
#SBATCH --error=job.%J.err
#SBATCH --output=job.%J.out
#SBATCH --time=00:10:00
#SBATCH --partition=standard


output_dir="Latest_Outputs/Dataset_64_64_64_3/"
mkdir -p "$output_dir"

mpirun -n 32 ./a.out data_files/data_64_64_64_3.bin.txt 4 4 2 64 64 64 3 ${output_dir}/output_4_4_2.txt

