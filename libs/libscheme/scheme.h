#ifndef SKENE_SCHEME_H
#define SKENE_SCHEME_H

#include <common/common.h>
#include <libstrview/string_view.h>
#include <libarena/arena.h>

typedef enum {
  SCHEME_VALUE_TAG_INT,
} scheme_value_tag_t;

typedef struct {
  scheme_value_tag_t tag;
  union {
    int32_t val;
  } as;
} scheme_value_t;

#define ERR_SCHEME_INVALID_TOKEN 1

int32_t scheme_eval(arena_t *arena, string_view_t input, scheme_value_t *output);


#endif
