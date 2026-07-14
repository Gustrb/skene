/* Compile swisstables.c straight into this translation unit. That makes its
   otherwise-file-local (PRIVATE == static) internals -- sw_group_match,
   sw_hash and the inline group scanners -- directly callable from the tests
   below, without widening the library's public surface. Because it is
   #included here, build_test.sh must NOT also link swisstables.c separately. */
#include <libtest/test.h>
#include <libstrview/string_view.h>
#include <libarena/arena.h>
#include <stdint.h>
#include <stdio.h>

#include "swisstables.c"

#define EMPTY ((int8_t)0x80)

/* Shorthand for building a string view from a C literal in a test. */
#define SV(s) string_view_from_cstr(s)

/* Per-test arena budget. Generous enough to absorb a table's growth plus the
   dead generations each resize abandons (see the header note on arena reuse). */
#define ARENA_CAP (1 << 20) /* 1 MiB */

void __assert_group_match_none(void);
void __assert_group_match_all_empty(void);
void __assert_group_match_single(void);
void __assert_group_match_multiple(void);
void __assert_bit_iteration(void);
void __assert_insert_find_roundtrip(void);
void __assert_update_existing(void);
void __assert_find_null_result(void);
void __assert_many_keys_collisions(void);
void __assert_resize_growth(void);
void __assert_resize_preserves_updates(void);
void __assert_insert_arena_oom(void);
void __assert_delete_basic(void);
void __assert_delete_absent(void);
void __assert_delete_empty_branch(void);
void __assert_delete_tombstone_and_chain(void);
void __assert_delete_then_reinsert(void);

/* Stand up a heap-backed arena and an arena-backed table for a test, and return
   the arena's backing buffer for the caller to free(). We manage the backing
   ourselves rather than using arena_new_with_capacity because the arena bumps
   `start` past the malloc base as it allocates, so the base can't be recovered
   from the arena afterwards. Returns NULL on failure; the table borrows `a`, so
   `a` must outlive it. */
static char *table_setup(sw_table_t *t, arena_t *a, size_t cap)
{
  char *backing = malloc(ARENA_CAP);
  if (!backing) return NULL;

  arena_new_with_underlying_buffer(a, backing, ARENA_CAP);
  if (sw_table_init(t, a, cap) != 0)
  {
    free(backing);
    return NULL;
  }
  return backing;
}

/* Which starting group a key maps to, for a table of `glen` groups.
   Mirrors the H1 split used inside sw_table_*: group = (hash >> 7) & (glen-1). */
static size_t key_group(const char *key, size_t glen)
{
  return (sw_hash(SV(key)) >> 7) & (glen - 1);
}

/* Fill `out[0..count)` with distinct keys that all map to `target_group`.
   Lets a test deterministically pack one group full to force the tombstone path. */
static void make_colliding_keys(char out[][16], int count, size_t glen,
                                size_t target_group)
{
  int found = 0;
  for (int n = 0; found < count; n++)
  {
    char tmp[16];
    snprintf(tmp, sizeof tmp, "c%d", n);
    if (key_group(tmp, glen) == target_group)
    {
      snprintf(out[found], 16, "%s", tmp);
      found++;
    }
  }
}

int main(void)
{
  TEST_START(78);

  __assert_group_match_none();
  __assert_group_match_all_empty();
  __assert_group_match_single();
  __assert_group_match_multiple();
  __assert_bit_iteration();

  __assert_insert_find_roundtrip();
  __assert_update_existing();
  __assert_find_null_result();
  __assert_many_keys_collisions();

  __assert_resize_growth();
  __assert_resize_preserves_updates();
  __assert_insert_arena_oom();

  __assert_delete_basic();
  __assert_delete_absent();
  __assert_delete_empty_branch();
  __assert_delete_tombstone_and_chain();
  __assert_delete_then_reinsert();

  TEST_FINISH();
  return 0;
}

void __assert_group_match_none(void)
{
  /* A fresh group: every byte EMPTY. No slot matches fingerprint 0x2a. */
  int8_t ctrl[SW_TABLE_GROUPSIZE] = {
    EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
  };
  ASSERT_INT_EQ(0x00, sw_group_match(ctrl, 0x2a),
                "sw_group_match returns 0 when no slot matches");
}

void __assert_group_match_all_empty(void)
{
  int8_t ctrl[SW_TABLE_GROUPSIZE] = {
    EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
  };
  /* All 8 slots are empty -> low 8 bits all set. */
  ASSERT_INT_EQ(0xff, sw_group_match(ctrl, EMPTY),
                "sw_group_match against EMPTY matches all 8 slots");
}

void __assert_group_match_single(void)
{
  int8_t ctrl[SW_TABLE_GROUPSIZE] = {
    EMPTY, EMPTY, 0x2a, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY,
  };
  /* Only slot 2 holds fingerprint 0x2a -> bit 2 set. */
  ASSERT_INT_EQ((1u << 2), sw_group_match(ctrl, 0x2a),
                "sw_group_match sets exactly the matching slot bit");
}

void __assert_group_match_multiple(void)
{
  int8_t ctrl[SW_TABLE_GROUPSIZE] = {
    0x2a, EMPTY, 0x2a, EMPTY, EMPTY, 0x2a, EMPTY, EMPTY,
  };
  /* Slots 0, 2, 5 share fingerprint 0x2a. */
  uint32_t expected = (1u << 0) | (1u << 2) | (1u << 5);
  ASSERT_INT_EQ(expected, sw_group_match(ctrl, 0x2a),
                "sw_group_match sets a bit for every matching slot");
}

void __assert_bit_iteration(void)
{
  /* Verify the ctz / clear-lowest-bit loop visits the right slots in order. */
  int8_t ctrl[SW_TABLE_GROUPSIZE] = {
    0x2a, EMPTY, 0x2a, EMPTY, EMPTY, 0x2a, EMPTY, EMPTY,
  };
  uint32_t mask = sw_group_match(ctrl, 0x2a);

  int visited[SW_TABLE_GROUPSIZE];
  int n = 0;
  while (mask)
  {
    visited[n++] = __builtin_ctz(mask); /* lowest set bit = slot index */
    mask &= mask - 1;                   /* clear that lowest set bit   */
  }

  ASSERT_INT_EQ(3, n, "the match mask iterates over exactly 3 slots");
  ASSERT_INT_EQ(0, visited[0], "first visited slot is 0");
  ASSERT_INT_EQ(2, visited[1], "second visited slot is 2");
  ASSERT_INT_EQ(5, visited[2], "third visited slot is 5");
}

void __assert_insert_find_roundtrip(void)
{
  sw_table_t t;
  arena_t a;
  char *backing = table_setup(&t, &a, 4);
  ASSERT_BOOL(backing != NULL, "sw_table_init succeeds"); /* 4 groups = 32 slots */

  /* Values are borrowed void* pointers; the payloads live on this frame and
     outlive every table access below. */
  int payload[3] = {1, 2, 3};
  ASSERT_INT_EQ(0, sw_table_insert(&t, SV("alpha"), &payload[0]), "insert alpha");
  ASSERT_INT_EQ(0, sw_table_insert(&t, SV("beta"), &payload[1]), "insert beta");
  ASSERT_INT_EQ(0, sw_table_insert(&t, SV("gamma"), &payload[2]), "insert gamma");

  void *out = NULL;
  ASSERT_BOOL(sw_table_find(&t, SV("alpha"), &out) == 1 && out == &payload[0], "find alpha -> its pointer");
  ASSERT_BOOL(sw_table_find(&t, SV("beta"), &out) == 1 && out == &payload[1], "find beta -> its pointer");
  ASSERT_BOOL(sw_table_find(&t, SV("gamma"), &out) == 1 && out == &payload[2], "find gamma -> its pointer");

  /* A key that was never inserted must report not-found. */
  ASSERT_INT_EQ(0, sw_table_find(&t, SV("missing"), &out), "find missing -> not found");

  ASSERT_INT_EQ(3, (int)t.resident, "three residents after three inserts");

  free(backing);
}

void __assert_update_existing(void)
{
  sw_table_t t;
  arena_t a;
  char *backing = table_setup(&t, &a, 4);
  ASSERT_BOOL(backing != NULL, "sw_table_init succeeds");

  int first = 1, second = 99;
  ASSERT_INT_EQ(0, sw_table_insert(&t, SV("alpha"), &first), "insert alpha");
  ASSERT_INT_EQ(0, sw_table_insert(&t, SV("alpha"), &second), "re-insert alpha updates"); /* same key -> update */

  void *out = NULL;
  ASSERT_BOOL(sw_table_find(&t, SV("alpha"), &out) == 1 && out == &second, "find alpha -> updated pointer");

  /* An update must NOT create a second entry. */
  ASSERT_INT_EQ(1, (int)t.resident, "update does not add a second entry");

  free(backing);
}

void __assert_find_null_result(void)
{
  sw_table_t t;
  arena_t a;
  char *backing = table_setup(&t, &a, 4);
  ASSERT_BOOL(backing != NULL, "sw_table_init succeeds");
  int payload = 1;
  ASSERT_INT_EQ(0, sw_table_insert(&t, SV("alpha"), &payload), "insert alpha");

  /* find must tolerate a NULL out-param (caller only wants existence). */
  ASSERT_INT_EQ(1, sw_table_find(&t, SV("alpha"), NULL), "find with NULL result reports present");
  ASSERT_INT_EQ(0, sw_table_find(&t, SV("missing"), NULL), "find with NULL result reports absent");

  free(backing);
}

void __assert_many_keys_collisions(void)
{
  sw_table_t t;
  arena_t a;
  char *backing = table_setup(&t, &a, 4);
  ASSERT_BOOL(backing != NULL, "sw_table_init succeeds"); /* 32 slots, limit = 28 */

  /* 20 keys forces multiple keys per group and exercises the probe walk
     across groups. Key and value storage live on this stack frame, which is
     fine because the table borrows the pointers and we never leave here. */
  enum { N = 20 };
  char keys[N][16];
  int payload[N];
  int all_inserted = 1;
  for (int i = 0; i < N; i++)
  {
    snprintf(keys[i], sizeof(keys[i]), "key-%d", i);
    payload[i] = i * 10;
    if (sw_table_insert(&t, SV(keys[i]), &payload[i]) != 0) all_inserted = 0;
  }
  ASSERT_BOOL(all_inserted, "all 20 keys insert successfully");

  ASSERT_INT_EQ(N, (int)t.resident, "resident count matches inserted keys");

  /* Every inserted key must read back the exact pointer it was stored with. */
  int all_found = 1;
  for (int i = 0; i < N; i++)
  {
    void *out = NULL;
    if (sw_table_find(&t, SV(keys[i]), &out) != 1 || out != &payload[i]) all_found = 0;
  }
  ASSERT_BOOL(all_found, "every inserted key reads back its exact pointer");

  /* Keys that look similar but were never inserted must miss. */
  void *out = NULL;
  ASSERT_INT_EQ(0, sw_table_find(&t, SV("key-20"), &out), "key-20 was never inserted");
  ASSERT_INT_EQ(0, sw_table_find(&t, SV("key--1"), &out), "key--1 was never inserted");

  free(backing);
}

void __assert_resize_growth(void)
{
  sw_table_t t;
  arena_t a;
  char *backing = table_setup(&t, &a, 1); /* 1 group = 8 slots, limit = 7 */
  ASSERT_BOOL(backing != NULL, "sw_table_init succeeds");

  /* Far more keys than the initial table holds -> forces several resizes. */
  enum { N = 100 };
  char keys[N][16];
  int payload[N];
  int all_inserted = 1;
  for (int i = 0; i < N; i++)
  {
    snprintf(keys[i], sizeof keys[i], "k-%d", i);
    payload[i] = i;
    if (sw_table_insert(&t, SV(keys[i]), &payload[i]) != 0) all_inserted = 0;
  }
  ASSERT_BOOL(all_inserted, "all 100 keys insert across resizes");

  ASSERT_INT_EQ(N, (int)t.resident, "resident count matches inserted keys");
  ASSERT_BOOL(t.groups_len > 1, "the table actually grew"); /* it actually grew */

  /* Every entry must survive the rehashes with its exact pointer. */
  int all_found = 1;
  for (int i = 0; i < N; i++)
  {
    void *out = NULL;
    if (sw_table_find(&t, SV(keys[i]), &out) != 1 || out != &payload[i]) all_found = 0;
  }
  ASSERT_BOOL(all_found, "every entry survives the rehashes with its pointer");

  /* A never-inserted key must still miss after growth. */
  void *out = NULL;
  ASSERT_INT_EQ(0, sw_table_find(&t, SV("k-100"), &out), "never-inserted key misses after growth");

  free(backing);
}

void __assert_resize_preserves_updates(void)
{
  sw_table_t t;
  arena_t a;
  char *backing = table_setup(&t, &a, 1);
  ASSERT_BOOL(backing != NULL, "sw_table_init succeeds");

  enum { N = 50 };
  char keys[N][16];
  int before_vals[N];
  int after_vals[N];
  for (int i = 0; i < N; i++)
  {
    snprintf(keys[i], sizeof keys[i], "k-%d", i);
    before_vals[i] = i;
    sw_table_insert(&t, SV(keys[i]), &before_vals[i]);
  }

  /* Update every key (to a distinct pointer) after the table has grown;
     resident must not change. */
  uint32_t before = t.resident;
  int all_updated = 1;
  for (int i = 0; i < N; i++)
  {
    after_vals[i] = i + 1000;
    if (sw_table_insert(&t, SV(keys[i]), &after_vals[i]) != 0) all_updated = 0;
  }
  ASSERT_BOOL(all_updated, "all updates succeed");
  ASSERT_INT_EQ((int)before, (int)t.resident, "updating existing keys does not change resident count");

  int all_found = 1;
  for (int i = 0; i < N; i++)
  {
    void *out = NULL;
    if (sw_table_find(&t, SV(keys[i]), &out) != 1 || out != &after_vals[i]) all_found = 0;
  }
  ASSERT_BOOL(all_found, "every key reads back its updated pointer");

  free(backing);
}

void __assert_insert_arena_oom(void)
{
  /* A tight stack arena sized to hold exactly the initial 1-group table but
     not a resized one. Filling the group to its limit forces a resize the
     arena can't satisfy; that insert must fail cleanly with SW_TABLE_ERR_NOMEM
     while leaving the already-inserted entries intact. */
  char buf[sizeof(metadata_t) + sizeof(group_t) + 32];
  arena_t a;
  arena_new_with_underlying_buffer(&a, buf, sizeof buf);

  sw_table_t t;
  ASSERT_INT_EQ(0, sw_table_init(&t, &a, 1), "init fits in a tight arena"); /* 1 group, limit 7 */

  char keys[SW_TABLE_GROUPSIZE][16];
  int payload[SW_TABLE_GROUPSIZE];
  for (int i = 0; i < SW_TABLE_GROUPSIZE; i++)
  {
    snprintf(keys[i], sizeof keys[i], "oom-%d", i);
    payload[i] = i;
  }

  /* The first 7 inserts fit the single group; the 8th trips the resize. */
  int early_ok = 1;
  for (int i = 0; i < 7; i++)
  {
    if (sw_table_insert(&t, SV(keys[i]), &payload[i]) != 0) early_ok = 0;
  }
  ASSERT_BOOL(early_ok, "inserts that fit the initial table succeed");

  ASSERT_INT_EQ(SW_TABLE_ERR_NOMEM, sw_table_insert(&t, SV(keys[7]), &payload[7]),
                "insert fails with NOMEM when the arena can't back a resize");

  /* The table must stay usable: everything inserted before the failure is
     still reachable, and the count reflects only the successful inserts. */
  int survivors_ok = 1;
  for (int i = 0; i < 7; i++)
  {
    void *out = NULL;
    if (sw_table_find(&t, SV(keys[i]), &out) != 1 || out != &payload[i]) survivors_ok = 0;
  }
  ASSERT_BOOL(survivors_ok, "entries inserted before the OOM survive the failed insert");
  ASSERT_INT_EQ(7, (int)t.resident, "resident count reflects only the successful inserts");

  /* Stack-backed arena: nothing to free. */
}

void __assert_delete_basic(void)
{
  sw_table_t t;
  arena_t a;
  char *backing = table_setup(&t, &a, 4);
  ASSERT_BOOL(backing != NULL, "sw_table_init succeeds");

  int payload[3] = {1, 2, 3};
  sw_table_insert(&t, SV("alpha"), &payload[0]);
  sw_table_insert(&t, SV("beta"), &payload[1]);
  sw_table_insert(&t, SV("gamma"), &payload[2]);

  ASSERT_INT_EQ(1, sw_table_delete(&t, SV("beta")), "delete beta reports removed");

  void *out = NULL;
  ASSERT_INT_EQ(0, sw_table_find(&t, SV("beta"), &out), "beta is gone after delete");
  ASSERT_BOOL(sw_table_find(&t, SV("alpha"), &out) == 1 && out == &payload[0], "alpha intact after delete");
  ASSERT_BOOL(sw_table_find(&t, SV("gamma"), &out) == 1 && out == &payload[2], "gamma intact after delete");
  ASSERT_INT_EQ(2, (int)t.resident, "resident drops to 2 after delete");

  free(backing);
}

void __assert_delete_absent(void)
{
  sw_table_t t;
  arena_t a;
  char *backing = table_setup(&t, &a, 4);
  ASSERT_BOOL(backing != NULL, "sw_table_init succeeds");
  int payload = 1;
  sw_table_insert(&t, SV("alpha"), &payload);

  ASSERT_INT_EQ(0, sw_table_delete(&t, SV("nope")), "deleting an absent key reports nothing removed");
  ASSERT_INT_EQ(1, (int)t.resident, "resident unchanged when deleting absent key");

  free(backing);
}

void __assert_delete_empty_branch(void)
{
  /* A single group with only a few keys always has empty slots, so a delete
     can free the slot outright -> no tombstone, dead stays 0. */
  sw_table_t t;
  arena_t a;
  char *backing = table_setup(&t, &a, 1);
  ASSERT_BOOL(backing != NULL, "sw_table_init succeeds");

  int payload[3] = {1, 2, 3};
  sw_table_insert(&t, SV("alpha"), &payload[0]);
  sw_table_insert(&t, SV("beta"), &payload[1]);
  sw_table_insert(&t, SV("gamma"), &payload[2]);
  ASSERT_INT_EQ(0, (int)t.dead, "no tombstones before any delete");

  ASSERT_INT_EQ(1, sw_table_delete(&t, SV("beta")), "delete beta reports removed");
  ASSERT_INT_EQ(0, (int)t.dead, "delete in a non-full group frees outright, no tombstone");
  ASSERT_INT_EQ(2, (int)t.resident, "resident drops to 2 after delete");

  void *out = NULL;
  ASSERT_INT_EQ(0, sw_table_find(&t, SV("beta"), &out), "beta is gone after delete");

  free(backing);
}

void __assert_delete_tombstone_and_chain(void)
{
  /* 2 groups = 16 slots, limit 14. Pack group 0 completely full (8 keys) plus
     one more key that overflows into group 1 -- its probe chain runs THROUGH
     group 0. Deleting from the full group 0 must tombstone (not empty), or the
     overflow key becomes unreachable. */
  sw_table_t t;
  arena_t a;
  char *backing = table_setup(&t, &a, 2);
  ASSERT_BOOL(backing != NULL, "sw_table_init succeeds");
  ASSERT_INT_EQ(2, (int)t.groups_len, "table starts with 2 groups");

  char keys[9][16];
  int payload[9];
  make_colliding_keys(keys, 9, t.groups_len, 0);
  for (int i = 0; i < 9; i++)
  {
    payload[i] = i;
    sw_table_insert(&t, SV(keys[i]), &payload[i]);
  }
  ASSERT_INT_EQ(2, (int)t.groups_len, "stayed under limit -> no resize"); /* stayed under limit -> no resize */
  ASSERT_INT_EQ(9, (int)t.resident, "nine residents after nine inserts");
  ASSERT_INT_EQ(0, (int)t.dead, "no tombstones before delete");

  /* Delete a key sitting in the now-full group 0. */
  ASSERT_INT_EQ(1, sw_table_delete(&t, SV(keys[0])), "delete from full group reports removed");
  ASSERT_INT_EQ(1, (int)t.dead, "tombstone path fires when the group is full"); /* tombstone path fired */
  ASSERT_INT_EQ(8, (int)t.resident, "resident drops to 8 after delete");

  void *out = NULL;
  ASSERT_INT_EQ(0, sw_table_find(&t, SV(keys[0]), &out), "the deleted key is gone");
  ASSERT_BOOL(sw_table_find(&t, SV(keys[8]), &out) == 1 && out == &payload[8], "the overflow key survives the tombstone");

  int rest_ok = 1;
  for (int i = 1; i < 9; i++)
  {
    if (sw_table_find(&t, SV(keys[i]), &out) != 1 || out != &payload[i]) rest_ok = 0;
  }
  ASSERT_BOOL(rest_ok, "every other key remains reachable through the tombstone");

  free(backing);
}

void __assert_delete_then_reinsert(void)
{
  /* Deleting then reinserting the same key must reuse space and not duplicate. */
  sw_table_t t;
  arena_t a;
  char *backing = table_setup(&t, &a, 4);
  ASSERT_BOOL(backing != NULL, "sw_table_init succeeds");

  int first = 1, second = 2;
  sw_table_insert(&t, SV("alpha"), &first);
  ASSERT_INT_EQ(1, sw_table_delete(&t, SV("alpha")), "delete alpha reports removed");
  ASSERT_INT_EQ(0, sw_table_insert(&t, SV("alpha"), &second), "reinsert alpha succeeds");

  void *out = NULL;
  ASSERT_BOOL(sw_table_find(&t, SV("alpha"), &out) == 1 && out == &second, "reinserted alpha holds the new pointer");
  ASSERT_INT_EQ(1, (int)t.resident, "reinsert reuses space, resident stays 1");

  free(backing);
}
