#!/bin/bash
# Exit immediately if a command fails
set -e

# Load variables
source ./scripts/config.sh

# Number of parallel build workers. Defaults to all cores; override with e.g.
#   JOBS=4 ./build_test.sh
JOBS="${JOBS:-$(nproc)}"

echo "Building with: $CFLAGS"
echo "Parallel jobs: $JOBS"

# Ensure build directories exist
mkdir -p build build/obj

LIBTESTBIN=build/libtest.o
LIBSTRVIEWBIN=build/libstrview.o
LIBARENABIN=build/libarena.o
LIBHASHTABLEBIN=build/libhashtable.o
LIBLOGBIN=build/log.o

# ---------------------------------------------------------------------------
# Shared support objects. These are prerequisites for the test binaries below,
# so they are built first, before the parallel phase.
# ---------------------------------------------------------------------------
$CC $CFLAGS libs/libtest/test.c -c -o $LIBTESTBIN $INCLUDES
$CC $CFLAGS libs/libstrview/string_view.c -c -o $LIBSTRVIEWBIN $INCLUDES
$CC $CFLAGS libs/libarena/arena.c -c -o $LIBARENABIN $INCLUDES
$CC $CFLAGS libs/libhashtable/swisstables.c -c -o $LIBHASHTABLEBIN $INCLUDES

# The whole logging library bundled into a single relocatable object. Each
# liblogging translation unit is compiled, then merged with `ld -r` so the test
# binaries can link one file ($LIBLOGBIN) instead of listing every source.
# NB: formatter_tests #includes formatter.c to reach its statics, so it must NOT
# link $LIBLOGBIN (which also contains formatter.c) — that would be a duplicate
# symbol. It stays on the plain strview+libtest objects below.
$CC $CFLAGS libs/liblogging/writer.c    -c -o build/obj/writer.o    $INCLUDES
$CC $CFLAGS libs/liblogging/handler.c   -c -o build/obj/handler.o   $INCLUDES
$CC $CFLAGS libs/liblogging/log.c       -c -o build/obj/log.o       $INCLUDES
$CC $CFLAGS libs/liblogging/formatter.c -c -o build/obj/formatter.o $INCLUDES
ld -r -o $LIBLOGBIN build/obj/writer.o build/obj/handler.o build/obj/log.o build/obj/formatter.o

# ---------------------------------------------------------------------------
# Test binaries. These are independent of one another, so they are built in
# parallel: each line below is one self-contained compile/link command, fed to
# xargs which runs up to $JOBS of them at a time. xargs exits non-zero if any
# command fails, which (via the `if`) fails the whole script.
# ---------------------------------------------------------------------------
test_builds() {
cat <<EOF
$CC $CFLAGS tools/elfview/elfview_tests.c -o build/elfview_tests tools/elfview/elf.c $LIBTESTBIN $INCLUDES
$CC $CFLAGS libs/libstrbuilder/strbuilder_tests.c -o build/strbuilder_tests libs/libstrbuilder/strbuilder.c $LIBTESTBIN $INCLUDES
$CC $CFLAGS libs/libstrview/string_view_tests.c -o build/string_view_tests $LIBSTRVIEWBIN $LIBTESTBIN $INCLUDES
$CC $CFLAGS libs/libarena/arena_tests.c -o build/arena_tests $LIBARENABIN $LIBTESTBIN $INCLUDES
$CC $CFLAGS libs/libencoding/toml/toml_parser_tests.c -o build/toml_parser_tests libs/libencoding/toml/toml_parser.c $LIBARENABIN $LIBHASHTABLEBIN $LIBSTRVIEWBIN $LIBTESTBIN $INCLUDES
$CC $CFLAGS libs/libhashtable/swisstables_tests.c -o build/swisstables_tests $LIBSTRVIEWBIN $LIBARENABIN $LIBTESTBIN $INCLUDES
$CC $CFLAGS libs/liblogging/writer_tests.c -o build/writer_tests $LIBLOGBIN $LIBSTRVIEWBIN $LIBTESTBIN $INCLUDES
$CC $CFLAGS libs/liblogging/formatter_tests.c -o build/formatter_tests $LIBSTRVIEWBIN $LIBTESTBIN $INCLUDES
$CC $CFLAGS libs/liblogging/handler_tests.c -o build/handler_tests $LIBLOGBIN $LIBSTRVIEWBIN $LIBTESTBIN $INCLUDES
$CC $CFLAGS libs/liblogging/log_tests.c -o build/log_tests $LIBLOGBIN $LIBSTRVIEWBIN $LIBTESTBIN $INCLUDES
EOF
}

if ! test_builds | xargs -P "$JOBS" -I CMD bash -c 'CMD'; then
  echo "Build failed." >&2
  exit 1
fi

echo "Build successful."
