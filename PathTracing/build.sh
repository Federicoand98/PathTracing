#!/bin/bash

# Build the project
echo "Building the project..."
# rm -rf build
cmake -S . -B build

cd build
make
cd -
