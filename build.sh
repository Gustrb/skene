#!/bin/bash
# Exit immediately if a command fails
set -e

# Load variables
source ./scripts/config.sh

echo "Building with: $CFLAGS"

# Ensure build directory exists
mkdir -p build

# 1. Build the ELF View tool
$CC $CFLAGS tools/elfview/main.c -o build/elfview $INCLUDES

echo "Build successful."

