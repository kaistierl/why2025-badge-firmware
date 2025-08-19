#!/bin/bash

# Simple CMake build script for sshterm

# Create build directory if it doesn't exist
mkdir -p build

# Configure and build the project
cd build
cmake ..
make

# Optionally run the executable
if [ "$1" = "run" ]; then
    echo "Running sshterm..."
    ./bin/sshterm
fi
