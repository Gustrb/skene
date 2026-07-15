#define _POSIX_C_SOURCE 200809L

/*
 * The formatter's interesting logic lives in static ("PRIVATE") helpers:
 * the RFC3339 timestamp renderer and its zero-padded integer writer. To reach
 * them we #include the translation unit directly, mirroring the swisstables
 * test's approach for exercising static internals.
 *
 * All writes go through a writer_t, so we install a capturing writer backed by
 * an in-memory buffer (via handle.ptr) and read back exactly what was emitted.
 *
 * NB: the console formatter emits the level after a two-space gap, so the
 * header renders as "[<ts>  <LEVEL>]: ".
 */

#include "formatter.c"

#include <libtest/test.h>
#include <string.h>

typedef struct {
  char buf[4096];
  size_t len;
} capture_t;

static int32_t capture_write(writer_t *w, string_view_t sv)
{
  capture_t *c = (capture_t *)w->handle.ptr;
  memcpy(c->buf + c->len, sv.addr, sv.length);
  c->len += sv.length;
  return 0;
}

static writer_t capture_writer(capture_t *c)
{
  c->len = 0;
  return (writer_t){ .handle.ptr = c, .write = capture_write, .flush = NULL };
}

static const char *capture_cstr(capture_t *c)
{
  c->buf[c->len] = '\0';
  return c->buf;
}

void __assert_u32_pad(void);
void __assert_rfc3339(void);
void __assert_field_value(void);
void __assert_level(void);
void __assert_console_formatter(void);

int main(void)
{
  TEST_START(27);

  __assert_u32_pad();
  __assert_rfc3339();
  __assert_field_value();
  __assert_level();
  __assert_console_formatter();

  TEST_FINISH();
  return 0;
}

void __assert_u32_pad(void)
{
  capture_t cap;
  writer_t w;

  w = capture_writer(&cap);
  writer_write_u32_pad(&w, 2024, 4);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "2024") == 0,
              "pad(2024, 4) should render the value unpadded when it fills the width");

  w = capture_writer(&cap);
  writer_write_u32_pad(&w, 5, 2);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "05") == 0,
              "pad(5, 2) should left-pad with a single zero");

  w = capture_writer(&cap);
  writer_write_u32_pad(&w, 7, 1);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "7") == 0,
              "pad(7, 1) should render a single digit with no padding");

  w = capture_writer(&cap);
  writer_write_u32_pad(&w, 123, 2);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "123") == 0,
              "pad(123, 2) should never truncate a value wider than the width");

  w = capture_writer(&cap);
  writer_write_u32_pad(&w, 100, 4);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "0100") == 0,
              "pad(100, 4) should left-pad a value that has trailing zeros");

  w = capture_writer(&cap);
  writer_write_u32_pad(&w, 0, 9);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "000000000") == 0,
              "pad(0, 9) should render nine zeros (nanosecond field width)");
}

void __assert_rfc3339(void)
{
  capture_t cap;
  writer_t w;

  w = capture_writer(&cap);
  writer_time_format_rfc3339(&w, 0);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "1970-01-01T00:00:00.000000000Z") == 0,
              "the unix epoch should format as 1970-01-01T00:00:00.000000000Z");

  w = capture_writer(&cap);
  writer_time_format_rfc3339(&w, 1136239445LL * 1000000000LL + 123456789LL);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "2006-01-02T22:04:05.123456789Z") == 0,
              "seconds and sub-second nanos should both render");

  w = capture_writer(&cap);
  writer_time_format_rfc3339(&w, 1700000000LL * 1000000000LL);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "2023-11-14T22:13:20.000000000Z") == 0,
              "a later timestamp with zero nanos should format correctly");
}

void __assert_field_value(void)
{
  capture_t cap;
  writer_t w;
  log_field_t f;

  w = capture_writer(&cap);
  f = LOG_FIELD_STRING("k", "hello");
  writer_write_field_value(&w, &f);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "hello") == 0,
              "a string field should render its contents verbatim");

  w = capture_writer(&cap);
  f = LOG_FIELD_INT64("k", 9223372036854775807LL);
  writer_write_field_value(&w, &f);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "9223372036854775807") == 0,
              "an int64 field should render INT64_MAX");

  w = capture_writer(&cap);
  f = LOG_FIELD_INT64("k", (-9223372036854775807LL - 1));
  writer_write_field_value(&w, &f);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "-9223372036854775808") == 0,
              "an int64 field should render INT64_MIN without overflow");

  w = capture_writer(&cap);
  f = LOG_FIELD_INT32("k", -42);
  writer_write_field_value(&w, &f);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "-42") == 0,
              "an int32 field should render a negative value with a leading '-'");

  w = capture_writer(&cap);
  f = LOG_FIELD_UINT64("k", 18446744073709551615ULL);
  writer_write_field_value(&w, &f);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "18446744073709551615") == 0,
              "a uint64 field should render UINT64_MAX unsigned");

  w = capture_writer(&cap);
  f = LOG_FIELD_DOUBLE("k", 3.5);
  writer_write_field_value(&w, &f);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "3.5") == 0,
              "a double field should render via %g");

  w = capture_writer(&cap);
  f = LOG_FIELD_PTR("k", (void *)0xdeadbeef);
  writer_write_field_value(&w, &f);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "0xdeadbeef") == 0,
              "a pointer field should render as lowercase 0x-prefixed hex");

  w = capture_writer(&cap);
  f = LOG_FIELD_PTR("k", NULL);
  writer_write_field_value(&w, &f);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "0x0") == 0,
              "a NULL pointer field should render as 0x0");
}

void __assert_level(void)
{
  capture_t cap;
  writer_t w;

  w = capture_writer(&cap);
  writer_write_level(&w, LOG_LEVEL_TRACE);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "TRACE") == 0, "LOG_LEVEL_TRACE renders as TRACE");

  w = capture_writer(&cap);
  writer_write_level(&w, LOG_LEVEL_DEBUG);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "DEBUG") == 0, "LOG_LEVEL_DEBUG renders as DEBUG");

  w = capture_writer(&cap);
  writer_write_level(&w, LOG_LEVEL_INFO);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "INFO") == 0, "LOG_LEVEL_INFO renders as INFO");

  w = capture_writer(&cap);
  writer_write_level(&w, LOG_LEVEL_WARN);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "WARN") == 0, "LOG_LEVEL_WARN renders as WARN");

  w = capture_writer(&cap);
  writer_write_level(&w, LOG_LEVEL_ERROR);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "ERROR") == 0, "LOG_LEVEL_ERROR renders as ERROR");

  w = capture_writer(&cap);
  writer_write_level(&w, LOG_LEVEL_FATAL);
  ASSERT_BOOL(strcmp(capture_cstr(&cap), "FATAL") == 0, "LOG_LEVEL_FATAL renders as FATAL");
}

void __assert_console_formatter(void)
{
  capture_t cap;
  log_formatter_fn_t fmt = console_formatter();

  /* No fields: "[<ts>  <LEVEL>]: <message>\n". */
  {
    writer_t w = capture_writer(&cap);
    log_record_t rec = {
      .timestamp = 0,
      .level = LOG_LEVEL_INFO,
      .message = string_view_from_cstr("hi"),
      .field_count = 0,
    };
    int rc = fmt(&rec, &w);
    ASSERT_INT_EQ(0, rc, "console formatter should return 0");
    ASSERT_BOOL(strcmp(capture_cstr(&cap),
                       "[1970-01-01T00:00:00.000000000Z INFO]: \"hi\"\n") == 0,
                "a field-less record should render timestamp, level and message");
  }

  /* One field: "<key>: <value>" is rendered after the message. */
  {
    writer_t w = capture_writer(&cap);
    log_record_t rec = {
      .timestamp = 0,
      .level = LOG_LEVEL_INFO,
      .message = string_view_from_cstr("hi"),
      .field_count = 1,
    };
    rec.fields[0] = LOG_FIELD_STRING("user", "root");
    fmt(&rec, &w);
    ASSERT_BOOL(strcmp(capture_cstr(&cap),
                       "[1970-01-01T00:00:00.000000000Z INFO]: \"hi\" (user=root)\n") == 0,
                "a single field should render its key and value");
  }

  /* Two fields of mixed types, joined by a single ", " separator. */
  {
    writer_t w = capture_writer(&cap);
    log_record_t rec = {
      .timestamp = 0,
      .level = LOG_LEVEL_INFO,
      .message = string_view_from_cstr("hi"),
      .field_count = 2,
    };
    rec.fields[0] = LOG_FIELD_STRING("a", "x");
    rec.fields[1] = LOG_FIELD_INT32("code", -7);
    fmt(&rec, &w);
    ASSERT_BOOL(strcmp(capture_cstr(&cap),
                       "[1970-01-01T00:00:00.000000000Z INFO]: \"hi\" (a=x, code=-7)\n") == 0,
                "two fields should be joined by a single ', ' separator and render values");
  }
}
