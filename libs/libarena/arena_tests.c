#include "arena.h"
#include <libtest/test.h>
#include <stdint.h>

void __assert_arena_new_with_capacity(void);
void __assert_arena_new_with_underlying_buffer(void);
void __assert_arena_alloc_returns_zeroed_in_bounds_memory(void);
void __assert_arena_alloc_advances_start(void);
void __assert_arena_alloc_respects_alignment(void);
void __assert_arena_alloc_allocations_do_not_overlap(void);
void __assert_arena_alloc_out_of_memory(void);
void __assert_arena_alloc_exact_fit(void);
void __assert_arena_alloc_zero_count(void);
void __assert_arena_new_single_object(void);
void __assert_arena_new_array(void);
void __assert_arena_new_dispatches_on_arity(void);
void __assert_arena_new_out_of_memory(void);

int main(void)
{
  TEST_START(31);

  __assert_arena_new_with_capacity();
  __assert_arena_new_with_underlying_buffer();
  __assert_arena_alloc_returns_zeroed_in_bounds_memory();
  __assert_arena_alloc_advances_start();
  __assert_arena_alloc_respects_alignment();
  __assert_arena_alloc_allocations_do_not_overlap();
  __assert_arena_alloc_out_of_memory();
  __assert_arena_alloc_exact_fit();
  __assert_arena_alloc_zero_count();
  __assert_arena_new_single_object();
  __assert_arena_new_array();
  __assert_arena_new_dispatches_on_arity();
  __assert_arena_new_out_of_memory();

  TEST_FINISH();
  return 0;
}

void __assert_arena_new_with_capacity(void)
{
  arena_t a = {0};
  int32_t err = arena_new_with_capacity(&a, 1024);

  ASSERT_INT_EQ(0, err, "arena_new_with_capacity should succeed");
  ASSERT_BOOL(a.start != NULL, "arena_new_with_capacity should allocate the backing buffer");
  ASSERT_INT_EQ(1024, (int)(a.end - a.start),
                "the arena span should equal the requested capacity");

  free(a.start);
}

void __assert_arena_new_with_underlying_buffer(void)
{
  // Adopting an existing buffer must point the arena at exactly that region,
  // without allocating anything of its own.
  char buff[64];
  arena_t a = {0};
  arena_new_with_underlying_buffer(&a, buff, sizeof buff);

  ASSERT_BOOL(a.start == buff, "start should point at the adopted buffer");
  ASSERT_BOOL(a.end == buff + sizeof buff, "end should sit just past the adopted buffer");
}

void __assert_arena_alloc_returns_zeroed_in_bounds_memory(void)
{
  char buff[128];
  arena_t a = {0};
  arena_new_with_underlying_buffer(&a, buff, sizeof buff);

  int *p = arena_alloc(&a, sizeof(int), _Alignof(int), 4);

  ASSERT_BOOL(p != NULL, "arena_alloc should succeed when there is room");
  ASSERT_BOOL((char *)p >= buff && (char *)(p + 4) <= buff + sizeof buff,
              "the allocation should lie entirely within the arena buffer");

  int all_zero = 1;
  for (int i = 0; i < 4; i++)
  {
    if (p[i] != 0) all_zero = 0;
  }
  ASSERT_BOOL(all_zero, "arena_alloc should zero the returned memory");

  // The region must be usable storage.
  p[0] = 1;
  p[3] = 42;
  ASSERT_INT_EQ(42, p[3], "the returned memory should be writable");
}

void __assert_arena_alloc_advances_start(void)
{
  // A byte-aligned allocation needs no padding, so start should advance by
  // exactly count * size.
  char buff[128];
  arena_t a = {0};
  arena_new_with_underlying_buffer(&a, buff, sizeof buff);

  char *before = a.start;
  arena_alloc(&a, sizeof(char), _Alignof(char), 10);

  ASSERT_INT_EQ(10, (int)(a.start - before),
                "allocating 10 bytes should advance start by 10");
}

void __assert_arena_alloc_respects_alignment(void)
{
  // Force the bump pointer off an 8-byte boundary, then demand an 8-aligned
  // allocation: the returned pointer must still be aligned.
  char buff[256];
  arena_t a = {0};
  arena_new_with_underlying_buffer(&a, buff, sizeof buff);

  arena_alloc(&a, 1, 1, 1); // waste a byte to (likely) misalign the pointer

  void *p = arena_alloc(&a, 8, 8, 1);

  ASSERT_BOOL(p != NULL, "an aligned allocation should succeed when there is room");
  ASSERT_BOOL(((uintptr_t)p & 7) == 0, "arena_alloc should honour the requested alignment");
}

void __assert_arena_alloc_allocations_do_not_overlap(void)
{
  // Two byte-aligned allocations should be laid out back-to-back with no gap
  // and no overlap.
  char buff[128];
  arena_t a = {0};
  arena_new_with_underlying_buffer(&a, buff, sizeof buff);

  char *first = arena_alloc(&a, 16, 1, 1);
  char *second = arena_alloc(&a, 16, 1, 1);

  ASSERT_BOOL(first != NULL && second != NULL, "both allocations should succeed");
  ASSERT_BOOL(second == first + 16, "the second allocation should directly follow the first");
}

void __assert_arena_alloc_out_of_memory(void)
{
  // Requesting more than the arena holds must fail cleanly and leave the
  // bump pointer untouched.
  char buff[16];
  arena_t a = {0};
  arena_new_with_underlying_buffer(&a, buff, sizeof buff);

  char *before = a.start;
  void *p = arena_alloc(&a, 1, 1, 100);

  ASSERT_BOOL(p == NULL, "an over-capacity allocation should return NULL");
  ASSERT_BOOL(a.start == before, "a failed allocation should not advance the arena");
}

void __assert_arena_alloc_exact_fit(void)
{
  // An allocation that consumes the whole arena should succeed, exhaust it,
  // and cause the next allocation to fail.
  char buff[32];
  arena_t a = {0};
  arena_new_with_underlying_buffer(&a, buff, sizeof buff);

  void *p = arena_alloc(&a, 1, 1, 32);
  ASSERT_BOOL(p != NULL, "an allocation that exactly fits should succeed");
  ASSERT_BOOL(a.start == a.end, "an exact-fit allocation should exhaust the arena");

  void *q = arena_alloc(&a, 1, 1, 1);
  ASSERT_BOOL(q == NULL, "allocating from an exhausted arena should return NULL");
}

void __assert_arena_alloc_zero_count(void)
{
  // A zero-count allocation is a no-op that still returns a valid pointer
  // without consuming any space.
  char buff[32];
  arena_t a = {0};
  arena_new_with_underlying_buffer(&a, buff, sizeof buff);

  char *before = a.start;
  void *p = arena_alloc(&a, 4, 4, 0);

  ASSERT_BOOL(p != NULL, "a zero-count allocation should return a valid pointer");
  ASSERT_BOOL(a.start == before, "a zero-count allocation should not advance the arena");
}

// The buffers below are over-aligned so the bump pointer starts on a 16-byte
// boundary: that removes any leading padding and makes the "consumed exactly
// N bytes" assertions on the arena_new macros deterministic.

void __assert_arena_new_single_object(void)
{
  // arena_new(a, T) allocates exactly one aligned, zeroed T.
  alignas(16) char buff[128];
  arena_t a = {0};
  arena_new_with_underlying_buffer(&a, buff, sizeof buff);

  char *before = a.start;
  int32_t *p = arena_new(&a, int32_t);

  ASSERT_BOOL(p != NULL, "arena_new(a, T) should allocate one object");
  ASSERT_BOOL(((uintptr_t)p & (alignof(int32_t) - 1)) == 0,
              "arena_new should return a suitably aligned object");
  ASSERT_INT_EQ(0, *p, "arena_new should zero the object");
  ASSERT_INT_EQ((int)sizeof(int32_t), (int)(a.start - before),
                "arena_new(a, T) should consume exactly sizeof(T)");
}

void __assert_arena_new_array(void)
{
  // arena_new(a, T, n) allocates n contiguous, zeroed T.
  alignas(16) char buff[128];
  arena_t a = {0};
  arena_new_with_underlying_buffer(&a, buff, sizeof buff);

  char *before = a.start;
  int32_t *p = arena_new(&a, int32_t, 3);

  ASSERT_BOOL(p != NULL, "arena_new(a, T, n) should allocate the array");
  ASSERT_INT_EQ(3 * (int)sizeof(int32_t), (int)(a.start - before),
                "arena_new(a, T, n) should consume n * sizeof(T)");

  int all_zero = 1;
  for (int i = 0; i < 3; i++)
  {
    if (p[i] != 0) all_zero = 0;
  }
  ASSERT_BOOL(all_zero, "arena_new array elements should be zeroed");
}

void __assert_arena_new_dispatches_on_arity(void)
{
  // The variadic dispatch must pick the single-object form for two args and
  // the array form for three, so the two consume different amounts.
  alignas(16) char buff[128];
  arena_t a = {0};
  arena_new_with_underlying_buffer(&a, buff, sizeof buff);

  char *b0 = a.start;
  arena_new(&a, int32_t);
  ASSERT_INT_EQ((int)sizeof(int32_t), (int)(a.start - b0),
                "the two-arg form should allocate a single element");

  char *b1 = a.start;
  arena_new(&a, int32_t, 4);
  ASSERT_INT_EQ(4 * (int)sizeof(int32_t), (int)(a.start - b1),
                "the three-arg form should allocate n elements");
}

void __assert_arena_new_out_of_memory(void)
{
  // The macros forward arena_alloc's failure, so an oversized request yields
  // NULL rather than overrunning the buffer.
  alignas(16) char buff[8];
  arena_t a = {0};
  arena_new_with_underlying_buffer(&a, buff, sizeof buff);

  int32_t *p = arena_new(&a, int32_t, 100);
  ASSERT_BOOL(p == NULL, "arena_new should return NULL when the arena is exhausted");
}
