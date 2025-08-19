#!/bin/bash

g++ main.cpp -o main

# Execute program1 5 times
echo "Running with hbanalysis.log 5 times:"
for i in {1..5}
do
    echo "Iteration $i"
    echo "Time Taken for DJIT:"
    ./main djit hbanalysis.log
    echo "Time Taken for FastTrack:"
    ./main fasttrack hbanalysis.log
done

