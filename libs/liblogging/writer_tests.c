#include "writer.h"
#include <libtest/test.h>
#include <string.h>

/*
 * The console writer's write/flush implementations operate on w->handle.file
 * generically, so we exercise them by taking a real console writer and swapping
 * its handle for a tmpfile(). That lets us read back exactly what was written
 * while only relying on the public writer_get_console_writer() entry point.
 */

void __assert_console_writer_shape(void);
void __assert_console_writer_write_bytes(void);
void __assert_console_writer_write_empty(void);
void __assert_console_writer_write_appends(void);
void __assert_console_writer_write_embedded_nul(void);
void __assert_console_writer_flush(void);

int main(void)
{
  TEST_START(21);

  __assert_console_writer_shape();
  __assert_console_writer_write_bytes();
  __assert_console_writer_write_empty();
  __assert_console_writer_write_appends();
  __assert_console_writer_write_embedded_nul();
  __assert_console_writer_flush();

  TEST_FINISH();
  return 0;
}

void __assert_console_writer_shape(void)
{
  writer_t w = writer_get_console_writer();

  ASSERT_BOOL(w.handle.file == stdout,
              "console writer should point at stdout");
  ASSERT_BOOL(w.write != NULL, "console writer should install a write fn");
  ASSERT_BOOL(w.flush != NULL, "console writer should install a flush fn");
}

void __assert_console_writer_write_bytes(void)
{
  FILE *tmp = tmpfile();
  ASSERT_BOOL(tmp != NULL, "tmpfile() should succeed");

  writer_t w = writer_get_console_writer();
  w.handle.file = tmp;

  string_view_t sv = string_view_from_cstr("hello");
  int32_t err = w.write(&w, sv);
  ASSERT_INT_EQ(0, err, "write should succeed for a normal string");

  fflush(tmp);
  rewind(tmp);
  char buf[16] = {0};
  size_t n = fread(buf, sizeof(char), sizeof(buf) - 1, tmp);
  ASSERT_INT_EQ(5, (int)n, "write should emit exactly the view's bytes");
  ASSERT_BOOL(memcmp(buf, "hello", 5) == 0,
              "written bytes should match the view contents");

  fclose(tmp);
}

void __assert_console_writer_write_empty(void)
{
  FILE *tmp = tmpfile();
  ASSERT_BOOL(tmp != NULL, "tmpfile() should succeed");

  writer_t w = writer_get_console_writer();
  w.handle.file = tmp;

  int32_t err = w.write(&w, string_view_empty());
  ASSERT_INT_EQ(0, err, "writing an empty view should succeed");

  fflush(tmp);
  rewind(tmp);
  char buf[8] = {0};
  size_t n = fread(buf, sizeof(char), sizeof(buf), tmp);
  ASSERT_INT_EQ(0, (int)n, "writing an empty view should emit no bytes");

  fclose(tmp);
}

void __assert_console_writer_write_appends(void)
{
  FILE *tmp = tmpfile();
  ASSERT_BOOL(tmp != NULL, "tmpfile() should succeed");

  writer_t w = writer_get_console_writer();
  w.handle.file = tmp;

  int32_t e1 = w.write(&w, string_view_from_cstr("foo"));
  int32_t e2 = w.write(&w, string_view_from_cstr("bar"));
  ASSERT_INT_EQ(0, e1, "first write should succeed");
  ASSERT_INT_EQ(0, e2, "second write should succeed");

  fflush(tmp);
  rewind(tmp);
  char buf[16] = {0};
  size_t n = fread(buf, sizeof(char), sizeof(buf) - 1, tmp);
  ASSERT_INT_EQ(6, (int)n, "successive writes should accumulate");
  ASSERT_BOOL(memcmp(buf, "foobar", 6) == 0,
              "successive writes should append in order");

  fclose(tmp);
}

void __assert_console_writer_write_embedded_nul(void)
{
  FILE *tmp = tmpfile();
  ASSERT_BOOL(tmp != NULL, "tmpfile() should succeed");

  writer_t w = writer_get_console_writer();
  w.handle.file = tmp;

  /* The writer is length-driven, not NUL-terminated, so embedded NULs
   * must be written verbatim. */
  const char raw[] = {'a', '\0', 'b'};
  string_view_t sv = {.addr = raw, .length = sizeof(raw)};
  int32_t err = w.write(&w, sv);
  ASSERT_INT_EQ(0, err, "write should succeed with an embedded NUL");

  fflush(tmp);
  rewind(tmp);
  char buf[8] = {0};
  size_t n = fread(buf, sizeof(char), sizeof(buf), tmp);
  ASSERT_INT_EQ(3, (int)n, "write should emit all bytes including the NUL");
  ASSERT_BOOL(memcmp(buf, raw, 3) == 0,
              "embedded NUL byte should be preserved");

  fclose(tmp);
}

void __assert_console_writer_flush(void)
{
  FILE *tmp = tmpfile();
  ASSERT_BOOL(tmp != NULL, "tmpfile() should succeed");

  writer_t w = writer_get_console_writer();
  w.handle.file = tmp;

  w.write(&w, string_view_from_cstr("buffered"));
  int32_t err = w.flush(&w);
  ASSERT_INT_EQ(0, err, "flush should succeed on a healthy stream");

  fclose(tmp);
}
