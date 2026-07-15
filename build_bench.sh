#!/bin/bash
# Exit immediately if a command fails
set -e

# Load variables
source ./scripts/config.sh

# Benchmarks are built in RELEASE mode, not the debug/sanitizer set: ASan adds
# huge overhead and -O0 measures un-optimized code, both of which poison timing.
# (-O2 is also why SKENE_BENCH_KEEP matters — that is when dead-code elimination
# would otherwise delete an unused loop body.)
BENCH_CFLAGS="$STD $WARNINGS $HARDENING $RELEASE_FLAGS"

echo "Building benchmarks with: $BENCH_CFLAGS"

mkdir -p build

# The runner links the framework (runner.c + bench.c) together with every
# benchmark translation unit and the libraries under test. Each SKENE_BENCH
# self-registers, so no benchmark is referenced by name anywhere here.
$CC $BENCH_CFLAGS \
  libs/libbenchmark/runner.c \
  libs/libbenchmark/bench.c \
  libs/libstrview/string_view_bench.c \
  libs/libstrview/string_view.c \
  -o build/bench_runner $INCLUDES

echo "Build successful. Run: ./build/bench_runner"
