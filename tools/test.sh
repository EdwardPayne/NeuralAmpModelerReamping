#!/bin/bash

# Set the number of loops
num_loops=14  # Change this to the desired number of loops

# Path to the binary
binary="./tools/reamp"

# Input files
nam_file="../testfiles/05-full-metal.nam"
input_file="../testfiles/first_5_seconds.wav"

# Loop through and execute the binary with increasing numbers
for ((i = 1; i <= num_loops; i++)); do
    output_file="../testfiles/output${i}.wav"
    echo "Executing loop $i: $binary $nam_file $input_file $output_file $i"
    $binary $nam_file $input_file $output_file $i
done
