#!/bin/zsh
set -e

source_file="${1:-main.cpp}"
executable="${source_file:t:r}"

mkdir -p build
pushd build

# TODO: -O0 suitable for debugging, not for benchmarking. Consider switch to
# -O2 or -O3.
clang++ -std=c++20 -g -O0 "../src/${source_file}" -o "${executable}"

popd
