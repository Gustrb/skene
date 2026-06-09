#ifndef SKENE_LIBSTRBUILDER_H
#define SKENE_LIBSTRBUILDER_H

#if !defined(__linux__) || !defined(__x86_64__)
#error "skene libstrbuilder supports only x86_64 on linux"
#endif

#include <common/common.h>
#include <libstrview/string_view.h>

typedef struct
{
  char *ptr;
  size_t length;
  size_t capacity;
} string_builder_t;

#define ERR_LIBSTRBUILDER_NOMEM 1024

PUBLIC int32_t string_builder_new(string_builder_t *);
PUBLIC int32_t string_builder_new_with_capacity(string_builder_t *, size_t);

PUBLIC int32_t string_builder_append_cstr(string_builder_t *builder, const char *cstr);

PUBLIC int32_t string_builder_append_string_view(string_builder_t *builder, string_view_t sv);
PUBLIC void string_builder_append_string_view_unchecked(string_builder_t *builder, string_view_t sv);

PUBLIC int32_t string_builder_from_cstr(string_builder_t *b, const char *str);
PUBLIC int32_t string_builder_into_owned_cstr(string_builder_t *b, const char **str);
PUBLIC int32_t string_builder_destroy(string_builder_t *);

#endif
