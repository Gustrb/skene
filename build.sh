#!/bin/bash
# Exit immediately if a command fails
set -e

# Load variables
source ./scripts/config.sh

echo "Building with: $CFLAGS"

# Ensure build directory exists
mkdir -p build

LIBSTRBUILDER=build/libstrbuilder.o

$CC $CFLAGS libs/libstrbuilder/strbuilder.c -c -o $LIBSTRBUILDER $INCLUDES

# 1. Build the ELF View tool
$CC $CFLAGS tools/elfview/main.c -o build/elfview tools/elfview/elf.c $INCLUDES
$CC $CFLAGS tools/asm-explorer/main.c -o build/asm-explorer $LIBSTRBUILDER $INCLUDES

echo "Build successful."

