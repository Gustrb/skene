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

/* `cap` is the desired capacity in elements; it is rounded up internally to a
   power-of-two number of SW_TABLE_GROUPSIZE-slot groups. */
int32_t sw_table_init(sw_table_t *table, arena_t *arena, size_t cap);
int32_t sw_table_find(sw_table_t *table, string_view_t key, void **result);
int32_t sw_table_insert(sw_table_t *table, string_view_t key, void *val);
int32_t sw_table_delete(sw_table_t *table, string_view_t key);
/* Drops every binding but keeps the storage, so the table can be refilled
   without going back to the arena. This is the only way to reuse a table when
   the arena has no free: re-running sw_table_init would bump a second set of
   arrays and strand the first. */
void sw_table_clear(sw_table_t *table);
/* Resets the handle. Reclaims nothing: the arena owns all storage and frees it
   wholesale when the caller discards the arena. */
void sw_table_free(sw_table_t *table);

/* Walks every live binding. Order is unspecified, and a resize reorders
   everything, so an iterator is only valid until the next insert. Deleting
   through one is likewise not supported. */
typedef struct {
  const sw_table_t *table;
  size_t group;
  size_t slot;
} sw_table_iter_t;

void sw_table_iter_new(sw_table_iter_t *iter, const sw_table_t *table);
/* Returns 1 and fills `key`/`value` (either may be NULL) while bindings remain,
   0 once the table is exhausted. */
int32_t sw_table_iter_next(sw_table_iter_t *iter, string_view_t *key, void **value);

#endif
