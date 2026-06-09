#ifndef SKENE_LIBSTRVIEW_H
#define SKENE_LIBSTRVIEW_H

#if !defined(__linux__) || !defined(__x86_64__)
#error "skene libstrview supports only x86_64 on linux"
#endif

#include <common/common.h>

typedef struct {
  const char *addr;
  size_t length;
} string_view_t;

typedef struct {
  string_view_t rest;
  char separator;
  uint8_t done;
} string_view_split_iter_t;

PUBLIC uint8_t string_view_equals(string_view_t a, string_view_t b);
PUBLIC uint8_t string_view_starts_with(string_view_t a, string_view_t b);
PUBLIC uint8_t string_view_starts_with_cstr(string_view_t a, const char *b);
PUBLIC string_view_t string_view_from_cstr(const char *a);

PUBLIC string_view_split_iter_t string_view_split(string_view_t a, char sep);
PUBLIC string_view_t string_view_split_iter_next(string_view_split_iter_t *svsi);

#endif
