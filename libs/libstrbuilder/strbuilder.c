#include <string.h>

#include "./strbuilder.h"

// TODO: refactor this to take in arena allocator or something like this
// those mallocs are really expensive

#define SKENE_LIBSTRBUILDER_DEFAULT_CAPACITY 32

PUBLIC int32_t string_builder_new(string_builder_t *builder)
{
  return string_builder_new_with_capacity(builder, SKENE_LIBSTRBUILDER_DEFAULT_CAPACITY);
}

PUBLIC int32_t string_builder_new_with_capacity(string_builder_t *builder, size_t cap)
{
  builder->capacity = cap;
  builder->length = 0;
  builder->ptr = malloc(sizeof(char) * builder->capacity);
  if (builder->ptr == NULL)
  {
    return ERR_LIBSTRBUILDER_NOMEM;
  }

  return 0;
}

PUBLIC int32_t string_builder_from_cstr(string_builder_t *b, const char *str)
{
  size_t cstr_len = strlen(str);
  int32_t err = string_builder_new_with_capacity(b, cstr_len);
  if (err != 0)
  {
    return err;
  }

  memcpy(b->ptr, str, b->capacity);
  b->length = cstr_len;

  return 0;
}

PUBLIC int32_t string_builder_into_owned_cstr(string_builder_t *b, const char **str)
{
  char *tmp = malloc(sizeof(char) * (b->length + 1));
  if (tmp == NULL)
  {
    return ERR_LIBSTRBUILDER_NOMEM;
  }

  memcpy(tmp, b->ptr, b->length);
  tmp[b->length] = '\0';

  *str = tmp;
  return 0;
}

PRIVATE int32_t string_builder_ensure_capacity(string_builder_t *builder, size_t cap)
{

  if (builder->capacity - builder->length < cap)
  {
    size_t new_cap = builder->capacity ? builder->capacity : 1;
    while (new_cap - builder->length < cap)
    {
      new_cap *= 2;
    }

    char *new_str = realloc(builder->ptr, new_cap);
    if (new_str == NULL)
    {
      return ERR_LIBSTRBUILDER_NOMEM;
    }

    builder->ptr = new_str;
    builder->capacity = new_cap;
  }

  return 0;
}

PUBLIC int32_t string_builder_append_cstr(string_builder_t *builder, const char *cstr)
{
  size_t cstr_len = strlen(cstr);
  int32_t err = string_builder_ensure_capacity(builder, cstr_len);
  if (err != 0)
  {
    return err;
  }
  
  memcpy(builder->ptr + builder->length, cstr, cstr_len);
  builder->length += cstr_len;
  return 0;
}

PUBLIC int32_t string_builder_append_string_view(string_builder_t *builder, string_view_t sv)
{
  int32_t err = string_builder_ensure_capacity(builder, sv.length);
  if (err != 0)
  {
    return err;
  }

  memcpy(builder->ptr + builder->length, sv.addr, sv.length);
  builder->length += sv.length;
  return 0;
}

PUBLIC int32_t string_builder_destroy(string_builder_t *b)
{
  free(b->ptr);
  b->length = 0;
  b->capacity = 0;
  return 0;
}
