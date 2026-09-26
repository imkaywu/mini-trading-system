#!/bin/zsh
set -e

mkdir -p build
pushd build

clang++ -std=c++20 -g -O0 ../src/main.cpp -o main

popd
