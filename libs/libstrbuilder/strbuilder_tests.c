#include "strbuilder.h"
#include <libtest/test.h>
#include <string.h>

void __assert_string_builder_new(void);
void __assert_string_builder_new_with_capacity(void);
void __assert_string_builder_new_with_zero_capacity(void);
void __assert_string_builder_from_cstr(void);
void __assert_string_builder_from_empty_cstr(void);
void __assert_string_builder_from_cstr_round_trips(void);
void __assert_string_builder_into_owned_cstr(void);
void __assert_string_builder_into_owned_empty(void);
void __assert_string_builder_into_owned_is_independent_copy(void);
void __assert_string_builder_append_cstr(void);
void __assert_string_builder_append_cstr_concatenates(void);
void __assert_string_builder_append_cstr_grows_capacity(void);
void __assert_string_builder_append_empty_cstr(void);
void __assert_string_builder_destroy(void);

int main(void)
{
  TEST_START(55);

  __assert_string_builder_new();
  __assert_string_builder_new_with_capacity();
  __assert_string_builder_new_with_zero_capacity();
  __assert_string_builder_from_cstr();
  __assert_string_builder_from_empty_cstr();
  __assert_string_builder_from_cstr_round_trips();
  __assert_string_builder_into_owned_cstr();
  __assert_string_builder_into_owned_empty();
  __assert_string_builder_into_owned_is_independent_copy();
  __assert_string_builder_append_cstr();
  __assert_string_builder_append_cstr_concatenates();
  __assert_string_builder_append_cstr_grows_capacity();
  __assert_string_builder_append_empty_cstr();
  __assert_string_builder_destroy();

  TEST_FINISH();
  return 0;
}

#define SKENE_LIBSTRBUILDER_DEFAULT_CAPACITY 32

void __assert_string_builder_new(void)
{
  string_builder_t b = {0};
  int32_t err = string_builder_new(&b);

  ASSERT_INT_EQ(0, err, "string_builder_new should succeed");
  ASSERT_INT_EQ(0, b.length, "a fresh builder should have length 0");
  ASSERT_INT_EQ(SKENE_LIBSTRBUILDER_DEFAULT_CAPACITY, b.capacity,
                "string_builder_new should use the default capacity");
  ASSERT_BOOL(b.ptr != NULL, "string_builder_new should allocate the backing buffer");

  string_builder_destroy(&b);
}

void __assert_string_builder_new_with_capacity(void)
{
  string_builder_t b = {0};
  int32_t err = string_builder_new_with_capacity(&b, 64);

  ASSERT_INT_EQ(0, err, "string_builder_new_with_capacity should succeed");
  ASSERT_INT_EQ(64, b.capacity, "the requested capacity should be honoured");
  ASSERT_INT_EQ(0, b.length, "a fresh builder should have length 0");
  ASSERT_BOOL(b.ptr != NULL, "string_builder_new_with_capacity should allocate the backing buffer");

  string_builder_destroy(&b);
}

void __assert_string_builder_new_with_zero_capacity(void)
{
  // A zero-capacity builder is a valid empty builder: no bytes yet, but the
  // call must still succeed and report a clean, empty state.
  string_builder_t b = {0};
  int32_t err = string_builder_new_with_capacity(&b, 0);

  ASSERT_INT_EQ(0, err, "string_builder_new_with_capacity(0) should succeed");
  ASSERT_INT_EQ(0, b.capacity, "a zero-capacity builder should report capacity 0");
  ASSERT_INT_EQ(0, b.length, "a zero-capacity builder should report length 0");

  string_builder_destroy(&b);
}

void __assert_string_builder_from_cstr(void)
{
  const char *src = "hello";
  string_builder_t b = {0};
  int32_t err = string_builder_from_cstr(&b, src);

  ASSERT_INT_EQ(0, err, "string_builder_from_cstr should succeed");
  ASSERT_INT_EQ((int)strlen(src), (int)b.length,
                "length should reflect the source string length");
  ASSERT_BOOL(b.capacity >= strlen(src),
              "capacity should be large enough to hold the source string");
  ASSERT_BOOL(b.ptr != NULL, "string_builder_from_cstr should allocate the backing buffer");
  ASSERT_BOOL(memcmp(b.ptr, src, strlen(src)) == 0,
              "the source bytes should be copied into the buffer");

  string_builder_destroy(&b);
}

void __assert_string_builder_from_empty_cstr(void)
{
  string_builder_t b = {0};
  int32_t err = string_builder_from_cstr(&b, "");

  ASSERT_INT_EQ(0, err, "string_builder_from_cstr(\"\") should succeed");
  ASSERT_INT_EQ(0, b.length, "an empty source string should yield length 0");

  string_builder_destroy(&b);
}

void __assert_string_builder_from_cstr_round_trips(void)
{
  // The key invariant: a string put in via from_cstr must come back out
  // unchanged via into_owned_cstr. This is what catches a missing length.
  const char *src = "the quick brown fox";
  string_builder_t b = {0};
  int32_t err = string_builder_from_cstr(&b, src);
  ASSERT_INT_EQ(0, err, "setup: from_cstr should succeed");

  const char *owned = NULL;
  err = string_builder_into_owned_cstr(&b, &owned);

  ASSERT_INT_EQ(0, err, "into_owned_cstr should succeed");
  ASSERT_BOOL(owned != NULL, "into_owned_cstr should allocate the result");
  ASSERT_BOOL(strcmp(owned, src) == 0, "from_cstr -> into_owned_cstr should round-trip the string");

  free((void *)owned);
  string_builder_destroy(&b);
}

void __assert_string_builder_into_owned_cstr(void)
{
  const char *src = "world";
  size_t src_len = strlen(src);

  string_builder_t b = {0};
  int32_t err = string_builder_new_with_capacity(&b, src_len);
  ASSERT_INT_EQ(0, err, "setup: builder allocation should succeed");

  memcpy(b.ptr, src, src_len);
  b.length = src_len;

  const char *owned = NULL;
  err = string_builder_into_owned_cstr(&b, &owned);

  ASSERT_INT_EQ(0, err, "string_builder_into_owned_cstr should succeed");
  ASSERT_BOOL(owned != NULL, "string_builder_into_owned_cstr should allocate the result");
  ASSERT_BOOL(strcmp(owned, src) == 0, "the owned string should match the builder contents");
  ASSERT_CHAR_EQ('\0', owned[src_len], "the owned string should be NUL-terminated");

  free((void *)owned);
  string_builder_destroy(&b);
}

void __assert_string_builder_into_owned_empty(void)
{
  // An empty builder should produce a valid, NUL-terminated empty string.
  string_builder_t b = {0};
  int32_t err = string_builder_new(&b);
  ASSERT_INT_EQ(0, err, "setup: builder allocation should succeed");

  const char *owned = NULL;
  err = string_builder_into_owned_cstr(&b, &owned);

  ASSERT_INT_EQ(0, err, "into_owned_cstr on an empty builder should succeed");
  ASSERT_BOOL(owned != NULL, "into_owned_cstr should allocate even for an empty builder");
  ASSERT_BOOL(strcmp(owned, "") == 0, "an empty builder should yield an empty string");

  free((void *)owned);
  string_builder_destroy(&b);
}

void __assert_string_builder_into_owned_is_independent_copy(void)
{
  // The owned string must be a real copy: destroying the builder (or mutating
  // its buffer) must not corrupt the previously returned string.
  const char *src = "detached";
  string_builder_t b = {0};
  int32_t err = string_builder_from_cstr(&b, src);
  ASSERT_INT_EQ(0, err, "setup: from_cstr should succeed");

  const char *owned = NULL;
  err = string_builder_into_owned_cstr(&b, &owned);
  ASSERT_INT_EQ(0, err, "setup: into_owned_cstr should succeed");

  // Tear the builder down; the owned copy must survive untouched.
  string_builder_destroy(&b);

  ASSERT_BOOL(owned != NULL && strcmp(owned, src) == 0,
              "the owned string should survive destruction of the builder");

  free((void *)owned);
}

void __assert_string_builder_append_cstr(void)
{
  // Appending to a fresh builder should record the bytes and advance the
  // length so the data can be recovered intact.
  const char *src = "hello";
  string_builder_t b = {0};
  int32_t err = string_builder_new(&b);
  ASSERT_INT_EQ(0, err, "setup: builder allocation should succeed");

  err = string_builder_append_cstr(&b, src);
  ASSERT_INT_EQ(0, err, "string_builder_append_cstr should succeed");
  ASSERT_INT_EQ((int)strlen(src), (int)b.length,
                "append should advance the length by the appended length");

  const char *owned = NULL;
  err = string_builder_into_owned_cstr(&b, &owned);
  ASSERT_INT_EQ(0, err, "into_owned_cstr should succeed");
  ASSERT_BOOL(owned != NULL && strcmp(owned, src) == 0,
              "the appended bytes should round-trip through into_owned_cstr");

  free((void *)owned);
  string_builder_destroy(&b);
}

void __assert_string_builder_append_cstr_concatenates(void)
{
  // Successive appends must accumulate, not overwrite.
  string_builder_t b = {0};
  int32_t err = string_builder_new(&b);
  ASSERT_INT_EQ(0, err, "setup: builder allocation should succeed");

  err = string_builder_append_cstr(&b, "foo");
  ASSERT_INT_EQ(0, err, "first append should succeed");
  err = string_builder_append_cstr(&b, "bar");
  ASSERT_INT_EQ(0, err, "second append should succeed");

  ASSERT_INT_EQ(6, (int)b.length, "length should be the sum of both appends");

  const char *owned = NULL;
  err = string_builder_into_owned_cstr(&b, &owned);
  ASSERT_BOOL(owned != NULL && strcmp(owned, "foobar") == 0,
              "successive appends should concatenate in order");

  free((void *)owned);
  string_builder_destroy(&b);
}

void __assert_string_builder_append_cstr_grows_capacity(void)
{
  // Appending more than the current capacity must trigger a grow while
  // preserving the contents.
  const char *src = "this string is longer than four bytes";
  size_t src_len = strlen(src);

  string_builder_t b = {0};
  int32_t err = string_builder_new_with_capacity(&b, 4);
  ASSERT_INT_EQ(0, err, "setup: small-capacity builder allocation should succeed");

  err = string_builder_append_cstr(&b, src);
  ASSERT_INT_EQ(0, err, "append larger than capacity should succeed");
  ASSERT_INT_EQ((int)src_len, (int)b.length, "length should reflect the appended string");
  ASSERT_BOOL(b.capacity >= b.length,
              "capacity should grow to at least hold the appended bytes");

  const char *owned = NULL;
  err = string_builder_into_owned_cstr(&b, &owned);
  ASSERT_BOOL(owned != NULL && strcmp(owned, src) == 0,
              "contents should survive a capacity grow");

  free((void *)owned);
  string_builder_destroy(&b);
}

void __assert_string_builder_append_empty_cstr(void)
{
  // Appending an empty string is a no-op: it succeeds and leaves the builder
  // untouched.
  string_builder_t b = {0};
  int32_t err = string_builder_from_cstr(&b, "abc");
  ASSERT_INT_EQ(0, err, "setup: from_cstr should succeed");

  err = string_builder_append_cstr(&b, "");
  ASSERT_INT_EQ(0, err, "appending an empty string should succeed");
  ASSERT_INT_EQ(3, (int)b.length, "appending an empty string should not change the length");

  string_builder_destroy(&b);
}

void __assert_string_builder_destroy(void)
{
  string_builder_t b = {0};
  string_builder_new(&b);

  int32_t err = string_builder_destroy(&b);

  ASSERT_INT_EQ(0, err, "string_builder_destroy should succeed");
  ASSERT_INT_EQ(0, b.length, "destroy should reset the length");
  ASSERT_INT_EQ(0, b.capacity, "destroy should reset the capacity");
}
