#!/bin/bash
#SBATCH -N 2
#SBATCH --ntasks-per-node=32
#SBATCH --error=master.err
#SBATCH --output=master.out
#SBATCH --time=00:20:00
#SBATCH --partition=standard

echo "Starting all jobs in sequence at: $(date)"

echo "Running job_64_64_64_3_8.sh"
bash job_files/job_64_64_64_3_8.sh

echo "Running job_64_64_64_3_16.sh"
bash job_files/job_64_64_64_3_16.sh

echo "Running job_64_64_64_3_32.sh"
bash job_files/job_64_64_64_3_32.sh

echo "Running job_64_64_64_3_64.sh"
bash job_files/job_64_64_64_3_64.sh

echo "Running job_64_64_96_7_8.sh"
bash job_files/job_64_64_96_7_8.sh

echo "Running job_64_64_96_7_16.sh"
bash job_files/job_64_64_96_7_16.sh

echo "Running job_64_64_96_7_32.sh"
bash job_files/job_64_64_96_7_32.sh

echo "Running job_64_64_96_7_64.sh"
bash job_files/job_64_64_96_7_64.sh

echo "Finished all jobs at: $(date)"
