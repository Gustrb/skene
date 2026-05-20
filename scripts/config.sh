#!/bin/bash

# Compiler Settings
CC="gcc"
STD="-std=c11"

# Warning Flags (The "Ironclad" set)
WARNINGS="-Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes -Werror"

# Security/Hardening
HARDENING="-fstack-protector-all"

# Context-dependent flags
# Switch these if you want to test "Release" mode
DEBUG_FLAGS="-g -O0 -fsanitize=address -fsanitize=undefined"
RELEASE_FLAGS="-O2 -DNDEBUG"

# Final Aggregations
CFLAGS="$STD $WARNINGS $HARDENING $DEBUG_FLAGS"
INCLUDES="-I./libs -I./include"

# Export them so sub-processes can see them if needed
export CC CFLAGS INCLUDES

