#include "string_view.h"
#include <libtest/test.h>

PRIVATE void __assert_can_compare_string_views(void);
PRIVATE void __assert_split_yields_whole_view_when_no_separator(void);
PRIVATE void __assert_split_yields_each_token(void);
PRIVATE void __assert_split_handles_trailing_separator(void);
PRIVATE void __assert_split_handles_leading_separator(void);
PRIVATE void __assert_split_on_empty_view_is_done(void);
PRIVATE void __assert_starts_with(void);
PRIVATE void __assert_into_i32_parses_valid_numbers(void);
PRIVATE void __assert_into_i32_rejects_invalid_input(void);
PRIVATE void __assert_into_i32_detects_overflow(void);

PRIVATE uint8_t __next_equals(string_view_split_iter_t *it, const char *expected);

int main(void)
{
  TEST_START(51);

  __assert_can_compare_string_views();
  __assert_split_yields_whole_view_when_no_separator();
  __assert_split_yields_each_token();
  __assert_split_handles_trailing_separator();
  __assert_split_handles_leading_separator();
  __assert_split_on_empty_view_is_done();
  __assert_starts_with();
  __assert_into_i32_parses_valid_numbers();
  __assert_into_i32_rejects_invalid_input();
  __assert_into_i32_detects_overflow();

  TEST_FINISH();
  return 0;
}

PRIVATE void __assert_can_compare_string_views(void)
{
  const char *a = "";
  const char *b = "";

  ASSERT_BOOL(string_view_equals(string_view_from_cstr(a), string_view_from_cstr(b)), "two empty strings should be equal");
  ASSERT_BOOL(string_view_equals(string_view_from_cstr(a), string_view_from_cstr(a)), "a string view should be equal to itself");

  a = "aa";
  b = "aa";
  ASSERT_BOOL(string_view_equals(string_view_from_cstr(a), string_view_from_cstr(b)), "two strings should be equal if the underlying memory is equal");

  a = "a";
  b = "aa";
  ASSERT_BOOL(string_view_equals(string_view_from_cstr(a), string_view_from_cstr(b)) == 0, "two different views are different if the strings are");

  a = "aa";
  b = "a";
  ASSERT_BOOL(string_view_equals(string_view_from_cstr(a), string_view_from_cstr(b)) == 0, "two different views are different if the strings are");
}

// Compares the next token produced by the iterator against the given cstr.
PRIVATE uint8_t __next_equals(string_view_split_iter_t *it, const char *expected)
{
  string_view_t got = string_view_split_iter_next(it);
  return string_view_equals(got, string_view_from_cstr(expected));
}

PRIVATE void __assert_split_yields_whole_view_when_no_separator(void)
{
  string_view_split_iter_t it = string_view_split(string_view_from_cstr("abc"), ',');

  ASSERT_BOOL(__next_equals(&it, "abc"), "a view without the separator yields the whole view");

  string_view_t after = string_view_split_iter_next(&it);
  ASSERT_INT_EQ(0, (int)after.length, "iterator is exhausted after the only token");
  ASSERT_BOOL(it.done, "iterator marks itself done once exhausted");
}

PRIVATE void __assert_split_yields_each_token(void)
{
  string_view_split_iter_t it = string_view_split(string_view_from_cstr("a,b,c"), ',');

  ASSERT_BOOL(__next_equals(&it, "a"), "first token of \"a,b,c\" is \"a\"");
  ASSERT_BOOL(__next_equals(&it, "b"), "second token of \"a,b,c\" is \"b\"");
  ASSERT_BOOL(__next_equals(&it, "c"), "third token of \"a,b,c\" is \"c\"");

  string_view_t after = string_view_split_iter_next(&it);
  ASSERT_INT_EQ(0, (int)after.length, "iterator is exhausted after the last token");
}

PRIVATE void __assert_split_handles_trailing_separator(void)
{
  string_view_split_iter_t it = string_view_split(string_view_from_cstr("a,"), ',');

  ASSERT_BOOL(__next_equals(&it, "a"), "first token of \"a,\" is \"a\"");
  ASSERT_BOOL(__next_equals(&it, ""), "a trailing separator yields a final empty token");

  string_view_t after = string_view_split_iter_next(&it);
  ASSERT_INT_EQ(0, (int)after.length, "iterator is exhausted after the trailing empty token");
}

PRIVATE void __assert_split_handles_leading_separator(void)
{
  string_view_split_iter_t it = string_view_split(string_view_from_cstr(",a"), ',');

  ASSERT_BOOL(__next_equals(&it, ""), "a leading separator yields a leading empty token");
  ASSERT_BOOL(__next_equals(&it, "a"), "second token of \",a\" is \"a\"");

  string_view_t after = string_view_split_iter_next(&it);
  ASSERT_INT_EQ(0, (int)after.length, "iterator is exhausted after the last token");
}

PRIVATE void __assert_split_on_empty_view_is_done(void)
{
  string_view_split_iter_t it = string_view_split(string_view_from_cstr(""), ',');

  string_view_t first = string_view_split_iter_next(&it);
  ASSERT_INT_EQ(0, (int)first.length, "an empty view yields no tokens");
  ASSERT_BOOL(it.done, "splitting an empty view marks the iterator done immediately");
}

PRIVATE void __assert_starts_with(void)
{
  string_view_t hello = string_view_from_cstr("hello");

  ASSERT_BOOL(string_view_starts_with(hello, string_view_from_cstr("he")),
              "\"hello\" starts with the proper prefix \"he\"");
  ASSERT_BOOL(string_view_starts_with(hello, string_view_from_cstr("hello")),
              "a view starts with itself");
  ASSERT_BOOL(string_view_starts_with(hello, string_view_from_cstr("")),
              "every view starts with the empty prefix");

  ASSERT_BOOL(string_view_starts_with(hello, string_view_from_cstr("hello world")) == 0,
              "a view does not start with a prefix longer than itself");
  ASSERT_BOOL(string_view_starts_with(hello, string_view_from_cstr("world")) == 0,
              "a non-matching prefix of equal length does not match");
  ASSERT_BOOL(string_view_starts_with(hello, string_view_from_cstr("ello")) == 0,
              "a substring that is not at the start is not a prefix");

  string_view_t empty = string_view_from_cstr("");
  ASSERT_BOOL(string_view_starts_with(empty, string_view_from_cstr("")),
              "the empty view starts with the empty prefix");
  ASSERT_BOOL(string_view_starts_with(empty, string_view_from_cstr("a")) == 0,
              "the empty view does not start with a non-empty prefix");
}

PRIVATE void __assert_into_i32_parses_valid_numbers(void)
{
  int32_t out = 0;

  ASSERT_INT_EQ(0, string_view_into_i32(string_view_from_cstr("0"), &out),
                "\"0\" parses without error");
  ASSERT_INT_EQ(0, out, "\"0\" parses to 0");

  ASSERT_INT_EQ(0, string_view_into_i32(string_view_from_cstr("123"), &out),
                "\"123\" parses without error");
  ASSERT_INT_EQ(123, out, "\"123\" parses to 123");

  string_view_into_i32(string_view_from_cstr("+7"), &out);
  ASSERT_INT_EQ(7, out, "a leading '+' is accepted");

  string_view_into_i32(string_view_from_cstr("-42"), &out);
  ASSERT_INT_EQ(-42, out, "a leading '-' yields a negative number");

  string_view_into_i32(string_view_from_cstr("007"), &out);
  ASSERT_INT_EQ(7, out, "leading zeros are ignored");

  string_view_into_i32(string_view_from_cstr("2147483647"), &out);
  ASSERT_INT_EQ(INT32_MAX, out, "INT32_MAX parses exactly");

  string_view_into_i32(string_view_from_cstr("-2147483648"), &out);
  ASSERT_BOOL(out == INT32_MIN, "INT32_MIN parses exactly");
}

PRIVATE void __assert_into_i32_rejects_invalid_input(void)
{
  int32_t out = 0;

  ASSERT_INT_EQ(ERR_STRVIEW_EMPTY, string_view_into_i32(string_view_empty(), &out),
                "an empty view is rejected");
  ASSERT_INT_EQ(ERR_STRVIEW_INVALID_DIGIT, string_view_into_i32(string_view_from_cstr("+"), &out),
                "a lone '+' is rejected");
  ASSERT_INT_EQ(ERR_STRVIEW_INVALID_DIGIT, string_view_into_i32(string_view_from_cstr("-"), &out),
                "a lone '-' is rejected");
  ASSERT_INT_EQ(ERR_STRVIEW_INVALID_DIGIT, string_view_into_i32(string_view_from_cstr("12a"), &out),
                "a trailing non-digit is rejected");
  ASSERT_INT_EQ(ERR_STRVIEW_INVALID_DIGIT, string_view_into_i32(string_view_from_cstr("a12"), &out),
                "a leading non-digit is rejected");
  ASSERT_INT_EQ(ERR_STRVIEW_INVALID_DIGIT, string_view_into_i32(string_view_from_cstr("1 2"), &out),
                "an embedded space is rejected");
  ASSERT_INT_EQ(ERR_STRVIEW_INVALID_DIGIT, string_view_into_i32(string_view_from_cstr("++1"), &out),
                "a doubled sign is rejected");
  ASSERT_INT_EQ(ERR_STRVIEW_INVALID_DIGIT, string_view_into_i32(string_view_from_cstr("12.5"), &out),
                "a fractional value is rejected");
}

PRIVATE void __assert_into_i32_detects_overflow(void)
{
  int32_t out = 0;

  ASSERT_INT_EQ(ERR_STRVIEW_OVERFLOW, string_view_into_i32(string_view_from_cstr("2147483648"), &out),
                "INT32_MAX + 1 overflows");
  ASSERT_INT_EQ(ERR_STRVIEW_OVERFLOW, string_view_into_i32(string_view_from_cstr("-2147483649"), &out),
                "INT32_MIN - 1 overflows");
  ASSERT_INT_EQ(ERR_STRVIEW_OVERFLOW, string_view_into_i32(string_view_from_cstr("9999999999"), &out),
                "a large positive value overflows");
  ASSERT_INT_EQ(ERR_STRVIEW_OVERFLOW, string_view_into_i32(string_view_from_cstr("-9999999999"), &out),
                "a large negative value overflows");
  ASSERT_INT_EQ(ERR_STRVIEW_OVERFLOW, string_view_into_i32(string_view_from_cstr("+99999999999999999999"), &out),
                "a very large value overflows without wrapping");

  out = 555;
  string_view_into_i32(string_view_from_cstr("2147483648"), &out);
  ASSERT_INT_EQ(555, out, "the out param is left untouched on overflow");
}
