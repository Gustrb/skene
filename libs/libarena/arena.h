#ifndef SKENE_LIBARENA_H
#define SKENE_LIBARENA_H

#if !defined(__linux__) || !defined(__x86_64__)
#error "skene libarena supports only x86_64 on linux"
#endif

#include <common/common.h>
#include <stddef.h>
#include <stdalign.h>

typedef struct {
  char *start;
  char *end;
  // TODO(api): allow a flags parameter so we can configure a couple things:
  // - growth strategy
  // - wheter to zero out memory or not
} arena_t;

#define ERR_LIBARENA_NOMEM 1

void arena_new_with_underlying_buffer(arena_t *, char *buff, ptrdiff_t buff_len);
int32_t arena_new_with_capacity(arena_t *, ptrdiff_t cap);
void *arena_alloc(arena_t *a, ptrdiff_t size, ptrdiff_t align, ptrdiff_t count);

#define arena_new(...) \
  arena_newx(__VA_ARGS__, arena_new3, arena_new2, _)(__VA_ARGS__)
#define arena_newx(a, b, c, d, ...) d
#define arena_new2(a, t)    (t *)arena_alloc(a, sizeof(t), alignof(t), 1)
#define arena_new3(a, t, n) (t *)arena_alloc(a, sizeof(t), alignof(t), n)

#endif
