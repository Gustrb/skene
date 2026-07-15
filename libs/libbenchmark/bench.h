#ifndef SKENE_BENCH_H
#define SKENE_BENCH_H

#include <common/common.h>
#include <libstrview/string_view.h>

/*
 * A benchmark receives a context carrying the fixed iteration count. It does
 * NOT time itself; the runner measures wall-clock time around the whole call
 * and divides by the iteration count to report cost-per-op.
 */
typedef struct {
  size_t iterations;
} bench_t;

typedef void (*bench_fn_t)(bench_t *);

/*
 * SKENE_BENCH(name) { <body> } defines a benchmark and auto-registers it.
 *
 * The expansion is three parts:
 *   1. a forward declaration of the benchmark function,
 *   2. a __constructor__ function that runs before main() and pushes
 *      {"name", fn} into the global store, and
 *   3. the benchmark function's signature WITHOUT a brace, so the { ... }
 *      you write after the macro becomes its body.
 *
 * The context parameter is always named `b`; refer to it (and pass it to
 * SKENE_BENCH_LOOP) from inside the body even though you never declared it.
 *
 * NB: the constructor and the benchmark function must have DIFFERENT names,
 * otherwise they are a conflicting redeclaration of the same identifier.
 * Registration also relies on the object file being linked directly into the
 * runner binary; do NOT bundle benchmarks into a static archive (.a) without
 * -Wl,--whole-archive, or the linker drops the unreferenced constructors.
 */
#define SKENE_BENCH(name)                                                      \
  static void __skene_bench_fn_##name(bench_t *b);                             \
  __attribute__((constructor))                                                 \
  static void __skene_bench_register_##name(void)                              \
  {                                                                            \
    bench_register(string_view_from_cstr(#name), __skene_bench_fn_##name);     \
  }                                                                            \
  static void __skene_bench_fn_##name(bench_t *b)

/* Runs the wrapped body b->iterations times. */
#define SKENE_BENCH_LOOP(b)                                                     \
  for (size_t __sk__i = 0; __sk__i < (b)->iterations; ++__sk__i)

/*
 * Optimization barrier: forces the compiler to treat `x` as observably used so
 * it cannot delete the loop body as dead code under -O2. Wrap whatever your hot
 * path produces, e.g. SKENE_BENCH_KEEP(result).
 */
#define SKENE_BENCH_KEEP(x) __asm__ volatile("" : : "g"(x) : "memory")

/* Appends {name, fn} to the global store. Called for you by SKENE_BENCH's
 * constructor before main() runs — you should never call this directly. */
PUBLIC void bench_register(string_view_t name, bench_fn_t fn);

/* Runs every registered benchmark and prints its per-op cost. */
PUBLIC int bench_run_all(void);

#endif
