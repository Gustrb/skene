#include <libbenchmark/bench.h>
#include <libstrview/string_view.h>

SKENE_BENCH(string_view_from_cstr)
{
  SKENE_BENCH_LOOP(b)
  {
    string_view_t sv = string_view_from_cstr("hello, world");
    SKENE_BENCH_KEEP(sv);
  }
}

SKENE_BENCH(string_view_equals)
{
  string_view_t a = string_view_from_cstr("hello, world");
  string_view_t c = string_view_from_cstr("hello, world");

  SKENE_BENCH_LOOP(b)
  {
    uint8_t eq = string_view_equals(a, c);
    SKENE_BENCH_KEEP(eq);
  }
}
