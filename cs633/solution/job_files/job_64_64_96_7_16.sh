#!/bin/bash
#SBATCH -N 1
#SBATCH --ntasks-per-node=16
#SBATCH --error=job.%J.err
#SBATCH --output=job.%J.out
#SBATCH --time=00:10:00
#SBATCH --partition=standard


output_dir="Latest_Outputs/Dataset_64_64_96_7/"
mkdir -p "$output_dir"

mpirun -n 16 ./a.out data_files/data_64_64_96_7.bin.txt 4 2 2 64 64 96 7 ${output_dir}/output_4_2_2.txt

