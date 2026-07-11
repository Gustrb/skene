#!/bin/bash
# Exit immediately if a command fails
set -e

# Load variables
source ./scripts/config.sh

echo "Building with: $CFLAGS"

# Ensure build directory exists
mkdir -p build

LIBTESTBIN=build/libtest.o
LIBSTRVIEWBIN=build/libstrview.o

$CC $CFLAGS libs/libtest/test.c -c -o $LIBTESTBIN $INCLUDES
$CC $CFLAGS libs/libstrview/string_view.c -c -o $LIBSTRVIEWBIN $INCLUDES

# 2. Build the ELF View tool
$CC $CFLAGS tools/elfview/elfview_tests.c -o build/elfview_tests tools/elfview/elf.c $LIBTESTBIN $INCLUDES

# 3. Build the string builder library tests
$CC $CFLAGS libs/libstrbuilder/strbuilder_tests.c -o build/strbuilder_tests libs/libstrbuilder/strbuilder.c $LIBTESTBIN $INCLUDES
$CC $CFLAGS libs/libstrview/string_view_tests.c -o build/string_view_tests $LIBSTRVIEWBIN $LIBTESTBIN $INCLUDES

# 4. Build the arena library tests
$CC $CFLAGS libs/libarena/arena_tests.c -o build/arena_tests libs/libarena/arena.c $LIBTESTBIN $INCLUDES

# 5. Build the toml parser library tests
$CC $CFLAGS libs/libencoding/toml/toml_parser_tests.c -o build/toml_parser_tests libs/libencoding/toml/toml_parser.c $LIBSTRVIEWBIN $LIBTESTBIN $INCLUDES

echo "Build successful."

