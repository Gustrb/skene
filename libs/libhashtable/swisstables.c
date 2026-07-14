#include "swisstables.h"

#include <string.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#endif

#define H1(hash) ((hash) >> 7)
#define H2(hash) ((hash) & 0x7f)

#define SW_TABLE_EMPTY_CONTROL 0x80
#define SW_TABLE_DELETED_CONTROL 0xFE

PRIVATE inline size_t sw_next_power_of_two(size_t v);
PRIVATE uint32_t sw_group_match(const int8_t *control, int8_t target);
PRIVATE inline uint32_t sw_group_match_h2(const int8_t *control, int8_t h2);
PRIVATE inline uint32_t sw_group_match_empty(const int8_t *control);
PRIVATE inline uint32_t sw_group_match_deleted(const int8_t *control);
PRIVATE int32_t sw_table_resize(sw_table_t *table);
PRIVATE uint64_t sw_hash(string_view_t key);
PRIVATE int32_t sw_table_init_groups(sw_table_t *table, arena_t *arena, size_t num_groups);

PUBLIC int32_t sw_table_init(sw_table_t *table, arena_t *arena, size_t cap)
{
  // `cap` is the desired capacity in *elements*. Each group holds
  // SW_TABLE_GROUPSIZE slots, so round the element count up to a whole number
  // of groups before provisioning the backing arrays.
  size_t num_groups = (cap + SW_TABLE_GROUPSIZE - 1) / SW_TABLE_GROUPSIZE;
  return sw_table_init_groups(table, arena, num_groups);
}

PRIVATE int32_t sw_table_init_groups(sw_table_t *table, arena_t *arena, size_t num_groups)
{
  // triangular probing only holds if the group count is a power of 2.
  size_t capacity = sw_next_power_of_two(num_groups);

  // Bump both arrays from the arena. arena_alloc zeroes its result, but an
  // empty control slot is 0x80 (not 0), so the controls still need memset.
  metadata_t *controls = arena_alloc(arena, sizeof(metadata_t),
                                     alignof(metadata_t), (ptrdiff_t)capacity);
  if (!controls)
  {
    return SW_TABLE_ERR_NOMEM;
  }
  memset(controls, SW_TABLE_EMPTY_CONTROL, capacity * sizeof(metadata_t));

  group_t *groups = arena_alloc(arena, sizeof(group_t),
                                alignof(group_t), (ptrdiff_t)capacity);
  if (!groups)
  {
    // No free: the controls region stays stranded in the arena until the
    // caller discards the whole arena.
    return SW_TABLE_ERR_NOMEM;
  }

  size_t slots = capacity * SW_TABLE_GROUPSIZE;

  table->arena      = arena;
  table->controls   = controls;
  table->groups     = groups;
  table->groups_len = capacity;
  table->resident   = 0;
  table->dead       = 0;
  table->limit      = slots - slots/8; /* 87.5% of the table */

  return 0;
}

PUBLIC int32_t sw_table_find(sw_table_t *table, string_view_t key, void **result)
{
  uint64_t h = sw_hash(key);
  // modulo trick. a % b == a & (b-1) if b is a power of 2
  size_t group_idx = H1(h) & (table->groups_len - 1);
  int8_t h2        = (int8_t)H2(h);
  size_t stride    = 0;

  for (;;)
  {
    const int8_t *controls = table->controls[group_idx];
    const group_t *group   = &table->groups[group_idx];

    uint32_t matches = sw_group_match_h2(controls, h2);
    while (matches)
    {
      // count the number of trailing 0's of the mask
      // i.e. where the next available slot is
      int slot = __builtin_ctz(matches);
      if (string_view_equals(group->keys[slot], key))
      {
        if (result) *result = group->values[slot];
        return 1;
      }
      matches &= matches - 1;
    }

    if (sw_group_match_empty(controls) != 0)
    {
      return 0;
    }

    stride++;
    // Triagular probing.
    // Since we have a power of 2 groups_len, we can be sure this can touch
    // every single group
    group_idx = (group_idx + stride) & (table->groups_len - 1);
  }
}

PUBLIC int32_t sw_table_insert(sw_table_t *table, string_view_t key, void *val)
{
  if (table->resident + table->dead >= table->limit)
  {
    if (sw_table_resize(table) != 0)
    {
      return SW_TABLE_ERR_NOMEM;
    }
  }

  uint64_t h = sw_hash(key);
  // modulo trick. a % b == a & (b-1) if b is a power of 2
  size_t group_idx = H1(h) & (table->groups_len - 1);
  int8_t h2        = (int8_t)H2(h);
  size_t stride    = 0;

  int found_insert = 0;
  size_t insert_group = 0;
  int insert_slot = 0;

  for (;;)
  {
    int8_t *controls = table->controls[group_idx];
    group_t *group   = &table->groups[group_idx];

    uint32_t matches = sw_group_match_h2(controls, h2);
    while (matches)
    {
      int slot = __builtin_ctz(matches);
      if (string_view_equals(group->keys[slot], key))
      {
        group->values[slot] = val;
        return 0;
      }
      matches &= matches - 1;
    }

    if (!found_insert)
    {
      uint32_t free = sw_group_match_empty(controls) | sw_group_match_deleted(controls);
      if (free)
      {
        found_insert = 1;
        insert_group = group_idx;
        insert_slot = __builtin_ctz(free);
      }
    }

    if (sw_group_match_empty(controls) != 0) break;

    stride++;
    group_idx = (group_idx + stride) & (table->groups_len - 1);
  }

  table->controls[insert_group][insert_slot] = h2;
  table->groups[insert_group].keys[insert_slot] = key;
  table->groups[insert_group].values[insert_slot] = val;
  table->resident++;
  
  return 0;
}

PUBLIC int32_t sw_table_delete(sw_table_t *table, string_view_t key)
{

  uint64_t h = sw_hash(key);
  // modulo trick. a % b == a & (b-1) if b is a power of 2
  size_t group_idx = H1(h) & (table->groups_len - 1);
  int8_t h2        = (int8_t)H2(h);
  size_t stride    = 0;

  for (;;)
  {
    int8_t *controls = table->controls[group_idx];
    const group_t *group   = &table->groups[group_idx];

    uint32_t matches = sw_group_match_h2(controls, h2);
    while (matches)
    {
      int slot = __builtin_ctz(matches);
      if (string_view_equals(group->keys[slot], key))
      {
        if (sw_group_match_empty(controls) != 0)
        {
          controls[slot] = (int8_t)SW_TABLE_EMPTY_CONTROL;
        }
        else
        {
          controls[slot] = (int8_t)SW_TABLE_DELETED_CONTROL;
          table->dead++;
        }

        table->resident--;
        return 1;
      }
      matches &= matches - 1;
    }

    if (sw_group_match_empty(controls) != 0)
    {
      return 0;
    }

    stride++;
    // Triagular probing.
    // Since we have a power of 2 groups_len, we can be sure this can touch
    // every single group
    group_idx = (group_idx + stride) & (table->groups_len - 1);
  }

  return 0;
}

PUBLIC void sw_table_free(sw_table_t *table)
{
  // The arena owns every allocation, so there is nothing to free here; we only
  // reset the handle. The backing memory is reclaimed when the caller discards
  // the arena. `arena` is left intact so the handle can be re-init'd against it.
  table->resident   = 0;
  table->limit      = 0;
  table->dead       = 0;
  table->groups_len = 0;
  table->controls   = NULL;
  table->groups     = NULL;
}

#if defined(__SSE2__)
/*
 * SSE2 group scan. A group is SW_TABLE_GROUPSIZE (8) control bytes, but an
 * SSE register is 16 bytes, so we use _mm_loadl_epi64 to load *exactly* 8
 * bytes (a single movq) and avoid reading past the end of the last group.
 * The upper 8 lanes are zeroed by the load; we discard them with & 0xff so a
 * target of 0x00 can't produce phantom matches there.
 */
PRIVATE uint32_t sw_group_match(const int8_t *control, int8_t target)
{
  __m128i ctrl  = _mm_loadl_epi64((const __m128i *)control);
  __m128i want  = _mm_set1_epi8(target);
  __m128i eq    = _mm_cmpeq_epi8(ctrl, want);
  /* movemask: bit i set iff lane i compared equal. Keep only the 8 real lanes. */
  return (uint32_t)(_mm_movemask_epi8(eq) & 0xff);
}
#else
PRIVATE uint32_t sw_group_match(const int8_t *control, int8_t target)
{
  uint32_t mask = 0;
  for (uint8_t i = 0; i < SW_TABLE_GROUPSIZE; ++i)
  {
    if (control[i] == target)
    {
      mask |= (1u << i);
    }
  }

  return mask;
}
#endif

PRIVATE inline uint32_t sw_group_match_h2(const int8_t *control, int8_t h2)
{
  return sw_group_match(control, h2);
}

PRIVATE inline uint32_t sw_group_match_empty(const int8_t *control)
{
  return sw_group_match(control, (int8_t)SW_TABLE_EMPTY_CONTROL);
}

PRIVATE inline uint32_t sw_group_match_deleted(const int8_t *control)
{
  return sw_group_match(control, (int8_t)SW_TABLE_DELETED_CONTROL);
}

inline size_t sw_next_power_of_two(size_t v)
{
  // overflow guard
  if (v <= 1) return 1;

  size_t p = 1;
  while (p < v)
  {
    p <<= 1;
  }
  return p;
}

PRIVATE uint64_t sw_hash(string_view_t key)
{
  uint64_t h = 1469598103934665603ULL; /* FNV offset basis */
  for (size_t i = 0; i < key.length; ++i)
  {
    h ^= (uint8_t)key.addr[i];
    h *= 1099511628211ULL; /* FNV prime */
  }
  return h;
}

PRIVATE int32_t sw_table_resize(sw_table_t *table)
{
  sw_table_t old = *table;
  // Resize reasons in group units, so it calls the group-based initializer
  // directly rather than sw_table_init (which expects an element count).
  size_t new_groups = (old.dead >= old.resident) ? old.groups_len : old.groups_len * 2;

  // The fresh generation is bump-allocated from the same arena; the old one is
  // simply abandoned there (no free). If the arena can't fit either the fresh
  // arrays or the rehash, we fail and leave `table` pointing at the still-valid
  // old generation so the caller sees a clean SW_TABLE_ERR_NOMEM.
  sw_table_t fresh;
  if (sw_table_init_groups(&fresh, old.arena, new_groups) != 0)
    return SW_TABLE_ERR_NOMEM;

  for (size_t g = 0; g < old.groups_len; ++g)
  {
    const int8_t *controls = old.controls[g];
    const group_t *grp = &old.groups[g];

    for (int s = 0; s < SW_TABLE_GROUPSIZE; ++s)
    {
      if (controls[s] >= 0)
      {
        if (sw_table_insert(&fresh, grp->keys[s], grp->values[s]) != 0)
        {
          return SW_TABLE_ERR_NOMEM;
        }
      }
    }
  }

  *table = fresh;
  return 0;
}
