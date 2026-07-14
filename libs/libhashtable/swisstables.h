#ifndef SWISSTABLES_H
#define SWISSTABLES_H

#include <libstrview/string_view.h>
#include <libarena/arena.h>
#include <common/common.h>

#define SW_TABLE_GROUPSIZE 8

/* Keys and values are both *borrowed*: the table stores the string_view_t key
   (whose bytes the caller owns) and an opaque void* value (whose pointee the
   caller owns). The table never copies or frees either. */
typedef struct {
  string_view_t keys[SW_TABLE_GROUPSIZE];
  void *values[SW_TABLE_GROUPSIZE];
} group_t;

typedef int8_t metadata_t[SW_TABLE_GROUPSIZE];

typedef struct {
  /* All internal storage is bump-allocated from this arena, which is the
     program's memory budget. A resize grabs a fresh region and abandons the
     old one (the arena has no per-allocation free); when the arena is
     exhausted, the triggering insert fails with SW_TABLE_ERR_NOMEM. */
  arena_t *arena;

  metadata_t *controls;
  group_t *groups;
  size_t groups_len;

  uint32_t resident;
  uint32_t dead;
  uint32_t limit;
} sw_table_t;

#define SW_TABLE_ERR_NOMEM -1

int32_t sw_table_init(sw_table_t *table, arena_t *arena, size_t cap);
int32_t sw_table_find(sw_table_t *table, string_view_t key, void **result);
int32_t sw_table_insert(sw_table_t *table, string_view_t key, void *val);
int32_t sw_table_delete(sw_table_t *table, string_view_t key);
/* Resets the handle. Reclaims nothing: the arena owns all storage and frees it
   wholesale when the caller discards the arena. */
void sw_table_free(sw_table_t *table);

#endif
