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

# 3. Build the string builder library tests
$CC $CFLAGS libs/libstrbuilder/strbuilder_tests.c -o build/strbuilder_tests libs/libstrbuilder/strbuilder.c $LIBTESTBIN $INCLUDES
$CC $CFLAGS libs/libstrview/string_view_tests.c -o build/string_view_tests libs/libstrview/string_view.c $LIBTESTBIN $INCLUDES

echo "Build successful."

