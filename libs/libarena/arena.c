#include <libarena/arena.h>

#include <stddef.h>
#include <stdint.h>
#include <string.h>

void arena_new_with_underlying_buffer(arena_t *a, char *buff, ptrdiff_t buff_len)
{
  a->start = buff;
  a->end = a->start + buff_len;
}

int32_t arena_new_with_capacity(arena_t *a, ptrdiff_t s)
{
  void *ptr = malloc(sizeof(char) * s);
  if (ptr == NULL)
  {
    return ERR_LIBARENA_NOMEM;
  }

  a->start = ptr;
  a->end = a->start ? a->start + s : 0;
  return 0;
}

__attribute((malloc))
void *arena_alloc(arena_t *a, ptrdiff_t size, ptrdiff_t align, ptrdiff_t count)
{
  ptrdiff_t padding = -(uintptr_t)a->start & (align - 1);
  ptrdiff_t available = a->end - a->start - padding;
  if (available < 0 || count > available/size)
  {
    // TODO: Introduce flag to enable allocating a new arena
    return NULL;
  }
  void *p = a->start + padding;
  a->start += padding + count*size;

  return memset(p, 0, count * size);
}
