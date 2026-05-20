#!/bin/bash
# Exit immediately if a command fails
set -e

# Load variables
source ./scripts/config.sh

echo "Building with: $CFLAGS"

# Ensure build directory exists
mkdir -p build

LIBTESTBIN=build/libtest.o

$CC $CFLAGS libs/libtest/test.c -c -o $LIBTESTBIN $INCLUDES

# 2. Build the ELF View tool
$CC $CFLAGS tools/elfview/elfview_tests.c -o build/elfview_tests tools/elfview/elf.c $LIBTESTBIN $INCLUDES

echo "Build successful."

