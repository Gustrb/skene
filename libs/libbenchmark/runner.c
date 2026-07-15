#include <libbenchmark/bench.h>

/*
 * The runner references no benchmark by name. Every SKENE_BENCH in a linked
 * object registers itself via a constructor before main() runs, so all this
 * has to do is drain the global store.
 */
int main(void)
{
  return bench_run_all();
}
