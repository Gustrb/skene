#define _POSIX_C_SOURCE 200809L

#include <libbenchmark/bench.h>

#include <time.h>

typedef struct  {
  string_view_t name;
  bench_fn_t fn;
} bench_entry_t;

#define SKENE_BENCH_MAX 256

PRIVATE bench_entry_t benches[SKENE_BENCH_MAX];
PRIVATE size_t        bench_count;

PUBLIC void bench_register(string_view_t name, bench_fn_t fn)
{
  if (bench_count >= SKENE_BENCH_MAX)
  {
    return;
  }

  benches[bench_count].name = name;
  benches[bench_count].fn = fn;
  bench_count++;
}

PRIVATE uint64_t now_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

#define N 1000000

/* Prints per_op nanoseconds using the smallest unit that keeps the value >= 1. */
PRIVATE void print_duration(uint64_t per_op)
{
  if (per_op < 1000ull)
  {
    printf("%lu ns/op\n", (unsigned long)per_op);
  }
  else if (per_op < 1000000ull)
  {
    printf("%.2f us/op\n", (double)per_op / 1e3);
  }
  else if (per_op < 1000000000ull)
  {
    printf("%.2f ms/op\n", (double)per_op / 1e6);
  }
  else
  {
    printf("%.2f s/op\n", (double)per_op / 1e9);
  }
}

PUBLIC int bench_run_all(void)
{
  for (size_t i = 0; i < bench_count; ++i)
  {
    bench_t b = { .iterations = N };

    uint64_t t0 = now_ns();
    benches[i].fn(&b);
    uint64_t t1 = now_ns();

    uint64_t per_op = (t1 - t0) / b.iterations;

    printf("%-32.*s ", (int)benches[i].name.length, benches[i].name.addr);
    print_duration(per_op);
  }

  return 0;
}
