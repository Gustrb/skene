/*
 * handler.c is a thin coordinator: it runs the configured formatter against a
 * record (writing through the handler's writer) and then flushes, returning the
 * flush result. It has no static internals, so we link the translation unit
 * directly rather than #include-ing it.
 *
 * To observe the coordination we install a fake formatter and a capturing
 * writer, both backed by a shared ctx_t via handle.ptr. That lets us assert
 * what the formatter saw, that flush ran exactly once *after* formatting, and
 * that the flush return value is propagated back to the caller.
 */

#include <liblogging/handler.h>

#include <libtest/test.h>
#include <string.h>

typedef struct {
  int write_calls;
  int flush_calls;
  int formatted_before_flush;
  const log_record_t *seen_record;
  int32_t flush_ret;
  char buf[256];
  size_t len;
} ctx_t;

static int32_t ctx_write(writer_t *w, string_view_t sv)
{
  ctx_t *c = (ctx_t *)w->handle.ptr;
  c->write_calls++;
  memcpy(c->buf + c->len, sv.addr, sv.length);
  c->len += sv.length;
  return 0;
}

static int32_t ctx_flush(writer_t *w)
{
  ctx_t *c = (ctx_t *)w->handle.ptr;
  c->flush_calls++;
  return c->flush_ret;
}

/* Fake formatter: records the record it saw, notes that it ran while flush had
 * not yet fired, and emits the message so the write path is exercised too. */
static int fake_formatter(const log_record_t *record, writer_t *writer)
{
  ctx_t *c = (ctx_t *)writer->handle.ptr;
  c->seen_record = record;
  c->formatted_before_flush = (c->flush_calls == 0);
  writer->write(writer, record->message);
  return 0;
}

static writer_t ctx_writer(ctx_t *c)
{
  return (writer_t){ .handle.ptr = c, .write = ctx_write, .flush = ctx_flush };
}

void __assert_handler_new(void);
void __assert_handler_write(void);

int main(void)
{
  TEST_START(9);

  __assert_handler_new();
  __assert_handler_write();

  TEST_FINISH();
  return 0;
}

void __assert_handler_new(void)
{
  ctx_t cap = {0};
  writer_t w = ctx_writer(&cap);
  log_handler_t h = log_handler_new(&w, fake_formatter);

  ASSERT_BOOL(h.writer == &w, "log_handler_new stores the writer pointer");
  ASSERT_BOOL(h.formatter == fake_formatter, "log_handler_new stores the formatter");
}

void __assert_handler_write(void)
{
  /* Happy path: formatter runs, writes, then flush returns success. */
  {
    ctx_t cap = {0};
    writer_t w = ctx_writer(&cap);
    log_handler_t h = log_handler_new(&w, fake_formatter);
    log_record_t rec = {
      .timestamp = 0,
      .level = LOG_LEVEL_INFO,
      .message = string_view_from_cstr("hello"),
      .field_count = 0,
    };

    int32_t rc = log_handler_write(&h, &rec);

    ASSERT_INT_EQ(0, rc, "log_handler_write returns the flush result (success)");
    ASSERT_BOOL(cap.seen_record == &rec, "the formatter receives the record it was given");
    ASSERT_BOOL(cap.write_calls > 0, "the formatter writes through the handler's writer");
    ASSERT_BOOL(cap.flush_calls == 1, "flush is invoked exactly once");
    ASSERT_BOOL(cap.formatted_before_flush, "formatting happens before the flush");
    ASSERT_BOOL(strncmp(cap.buf, "hello", cap.len) == 0 && cap.len == 5,
                "the record's message reaches the writer");
  }

  /* Error path: a failing flush is propagated verbatim to the caller. */
  {
    ctx_t cap = {0};
    cap.flush_ret = WRITER_ERR_FAILED_TO_FLUSH;
    writer_t w = ctx_writer(&cap);
    log_handler_t h = log_handler_new(&w, fake_formatter);
    log_record_t rec = {
      .timestamp = 0,
      .level = LOG_LEVEL_INFO,
      .message = string_view_from_cstr("x"),
      .field_count = 0,
    };

    int32_t rc = log_handler_write(&h, &rec);

    ASSERT_INT_EQ(WRITER_ERR_FAILED_TO_FLUSH, rc,
                  "log_handler_write propagates a flush failure");
  }
}
