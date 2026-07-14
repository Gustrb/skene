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
LIBARENABIN=build/libarena.o
LIBHASHTABLEBIN=build/libhashtable.o

$CC $CFLAGS libs/libtest/test.c -c -o $LIBTESTBIN $INCLUDES
$CC $CFLAGS libs/libstrview/string_view.c -c -o $LIBSTRVIEWBIN $INCLUDES
$CC $CFLAGS libs/libarena/arena.c -c -o $LIBARENABIN $INCLUDES
$CC $CFLAGS libs/libhashtable/swisstables.c -c -o $LIBHASHTABLEBIN $INCLUDES

# 2. Build the ELF View tool
$CC $CFLAGS tools/elfview/elfview_tests.c -o build/elfview_tests tools/elfview/elf.c $LIBTESTBIN $INCLUDES

# 3. Build the string builder library tests
$CC $CFLAGS libs/libstrbuilder/strbuilder_tests.c -o build/strbuilder_tests libs/libstrbuilder/strbuilder.c $LIBTESTBIN $INCLUDES
$CC $CFLAGS libs/libstrview/string_view_tests.c -o build/string_view_tests $LIBSTRVIEWBIN $LIBTESTBIN $INCLUDES

# 4. Build the arena library tests
$CC $CFLAGS libs/libarena/arena_tests.c -o build/arena_tests $LIBARENABIN $LIBTESTBIN $INCLUDES

# 5. Build the toml parser library tests
$CC $CFLAGS libs/libencoding/toml/toml_parser_tests.c -o build/toml_parser_tests libs/libencoding/toml/toml_parser.c $LIBARENABIN $LIBHASHTABLEBIN $LIBSTRVIEWBIN $LIBTESTBIN $INCLUDES

# 6. Build the hash table (swiss tables) library tests
# NB: swisstables.c is #included by the test (see its header comment) to reach
# the static internals, so it must NOT be listed as a separate source here.
$CC $CFLAGS libs/libhashtable/swisstables_tests.c -o build/swisstables_tests $LIBSTRVIEWBIN $LIBARENABIN $LIBTESTBIN $INCLUDES

# 7. Build the logging writer library tests
$CC $CFLAGS libs/liblogging/writer_tests.c -o build/writer_tests libs/liblogging/writer.c $LIBSTRVIEWBIN $LIBTESTBIN $INCLUDES

echo "Build successful."

