/*
 * log.c coordinates a logger over a fixed set of handlers: it registers
 * handlers (bounded by LOG_MAX_HANDLERS), stamps records with time_now() on
 * emit, and fans a record out to every handler. It has no static internals, so
 * we link the translation unit directly (along with handler.c and formatter.c,
 * which supplies time_now()).
 *
 * To observe the fan-out we install real handlers whose formatter and writer
 * are fakes backed by a probe_t (via handle.ptr). Each handler records what
 * record it was asked to format, so we can assert the emitted record's fields
 * and that every registered handler saw it.
 */

#include <liblogging/log.h>

#include <libtest/test.h>
#include <string.h>

typedef struct {
  int calls;
  log_level_t level;
  string_view_t message;
  uint8_t field_count;
  log_field_t fields[LOG_MAX_FIELDS_PER_RECORD];
  timestamp_t timestamp;
} probe_t;

static int32_t probe_write(writer_t *w, string_view_t sv)
{
  UNUSED(w);
  UNUSED(sv);
  return 0;
}

static int32_t probe_flush(writer_t *w)
{
  UNUSED(w);
  return 0;
}

/* Fake formatter: snapshots the record so the test can inspect it afterwards. */
static int probe_formatter(const log_record_t *record, writer_t *writer)
{
  probe_t *p = (probe_t *)writer->handle.ptr;
  p->calls++;
  p->level = record->level;
  p->message = record->message;
  p->field_count = record->field_count;
  p->timestamp = record->timestamp;
  memcpy(p->fields, record->fields, record->field_count * sizeof(log_field_t));
  return 0;
}

static writer_t probe_writer(probe_t *p)
{
  return (writer_t){ .handle.ptr = p, .write = probe_write, .flush = probe_flush };
}

void __assert_logger_new(void);
void __assert_add_handler(void);
void __assert_emit(void);
void __assert_log_macros(void);

int main(void)
{
  TEST_START(30);

  __assert_logger_new();
  __assert_add_handler();
  __assert_emit();
  __assert_log_macros();

  TEST_FINISH();
  return 0;
}

void __assert_logger_new(void)
{
  logger_t logger = log_logger_new();
  ASSERT_INT_EQ(0, (int)logger.handler_count, "a fresh logger has no handlers");
}

void __assert_add_handler(void)
{
  logger_t logger = log_logger_new();

  probe_t p1 = {0}, p2 = {0}, p3 = {0};
  writer_t w1 = probe_writer(&p1), w2 = probe_writer(&p2), w3 = probe_writer(&p3);
  log_handler_t h1 = log_handler_new(&w1, probe_formatter);
  log_handler_t h2 = log_handler_new(&w2, probe_formatter);
  log_handler_t h3 = log_handler_new(&w3, probe_formatter);

  ASSERT_INT_EQ(0, log_logger_add_handler(&logger, &h1), "adding the first handler succeeds");
  ASSERT_INT_EQ(1, (int)logger.handler_count, "the handler count increments");
  ASSERT_BOOL(logger.handlers[0] == &h1, "the handler pointer is stored");

  ASSERT_INT_EQ(0, log_logger_add_handler(&logger, &h2), "adding a second handler succeeds");
  ASSERT_INT_EQ(2, (int)logger.handler_count, "the count reaches LOG_MAX_HANDLERS");

  ASSERT_INT_EQ(LOGGER_ERR_TOO_MANY_HANDLERS, log_logger_add_handler(&logger, &h3),
                "adding past LOG_MAX_HANDLERS is rejected");
  ASSERT_INT_EQ(2, (int)logger.handler_count, "a rejected handler does not change the count");
}

void __assert_emit(void)
{
  /* Reject records that carry too many fields, before any handler runs. */
  {
    logger_t logger = log_logger_new();
    probe_t p = {0};
    writer_t w = probe_writer(&p);
    log_handler_t h = log_handler_new(&w, probe_formatter);
    log_logger_add_handler(&logger, &h);

    int32_t rc = log_emit(&logger, LOG_LEVEL_INFO, string_view_from_cstr("m"),
                          NULL, LOG_MAX_FIELDS_PER_RECORD + 1);
    ASSERT_INT_EQ(LOG_ERR_TOO_MANY_FIELDS, rc, "emitting more than LOG_MAX_FIELDS_PER_RECORD is rejected");
    ASSERT_INT_EQ(0, p.calls, "a rejected emit never reaches a handler");
  }

  /* A record may fill the field array to capacity (LOG_MAX_FIELDS_PER_RECORD). */
  {
    logger_t logger = log_logger_new();
    probe_t p = {0};
    writer_t w = probe_writer(&p);
    log_handler_t h = log_handler_new(&w, probe_formatter);
    log_logger_add_handler(&logger, &h);

    log_field_t fields[LOG_MAX_FIELDS_PER_RECORD];
    for (size_t i = 0; i < LOG_MAX_FIELDS_PER_RECORD; ++i)
    {
      fields[i] = LOG_FIELD_INT32("n", (int32_t)i);
    }
    int32_t rc = log_emit(&logger, LOG_LEVEL_INFO, string_view_from_cstr("full"),
                          fields, LOG_MAX_FIELDS_PER_RECORD);

    ASSERT_INT_EQ(0, rc, "a record filled to LOG_MAX_FIELDS_PER_RECORD is accepted");
    ASSERT_BOOL(p.field_count == LOG_MAX_FIELDS_PER_RECORD,
                "every field up to capacity is copied into the record");
  }

  /* A successful emit builds a record and fans it out to every handler. */
  {
    logger_t logger = log_logger_new();
    probe_t p1 = {0}, p2 = {0};
    writer_t w1 = probe_writer(&p1), w2 = probe_writer(&p2);
    log_handler_t h1 = log_handler_new(&w1, probe_formatter);
    log_handler_t h2 = log_handler_new(&w2, probe_formatter);
    log_logger_add_handler(&logger, &h1);
    log_logger_add_handler(&logger, &h2);

    log_field_t fields[1] = { LOG_FIELD_INT32("code", 42) };
    int32_t rc = log_emit(&logger, LOG_LEVEL_WARN, string_view_from_cstr("hi"), fields, 1);

    ASSERT_INT_EQ(0, rc, "a well-formed emit succeeds");
    ASSERT_INT_EQ(1, p1.calls, "the first handler formats the record exactly once");
    ASSERT_INT_EQ(1, p2.calls, "the emit fans out to every registered handler");
    ASSERT_BOOL(p1.level == LOG_LEVEL_WARN, "the record carries the emitted level");
    ASSERT_BOOL(string_view_equals(p1.message, string_view_from_cstr("hi")),
                "the record carries the emitted message");
    ASSERT_BOOL(p1.field_count == 1 && p1.fields[0].as.i32 == 42 &&
                string_view_equals(p1.fields[0].key, string_view_from_cstr("code")),
                "the emitted field is copied into the record");
    ASSERT_BOOL(p1.timestamp > 0, "the record is stamped with a wall-clock timestamp");
  }
}

/*
 * The LOG_<LEVEL> family are thin wrappers that pin the level and dispatch on
 * argument count so the *same* macro serves calls with and without attributes.
 * We assert each wrapper stamps its record with the matching level, then that
 * the zero-, one-, and two-attribute forms all route through and forward their
 * message and fields intact.
 */
void __assert_log_macros(void)
{
  logger_t logger = log_logger_new();
  probe_t p = {0};
  writer_t w = probe_writer(&p);
  log_handler_t h = log_handler_new(&w, probe_formatter);
  log_logger_add_handler(&logger, &h);

  LOG_TRACE(&logger, "m", LOG_FIELD_INT32("x", 0));
  ASSERT_BOOL(p.level == LOG_LEVEL_TRACE, "LOG_TRACE emits at LOG_LEVEL_TRACE");

  LOG_DEBUG(&logger, "m", LOG_FIELD_INT32("x", 0));
  ASSERT_BOOL(p.level == LOG_LEVEL_DEBUG, "LOG_DEBUG emits at LOG_LEVEL_DEBUG");

  LOG_INFO(&logger, "m", LOG_FIELD_INT32("x", 0));
  ASSERT_BOOL(p.level == LOG_LEVEL_INFO, "LOG_INFO emits at LOG_LEVEL_INFO");

  LOG_WARN(&logger, "m", LOG_FIELD_INT32("x", 0));
  ASSERT_BOOL(p.level == LOG_LEVEL_WARN, "LOG_WARN emits at LOG_LEVEL_WARN");

  LOG_ERROR(&logger, "m", LOG_FIELD_INT32("x", 0));
  ASSERT_BOOL(p.level == LOG_LEVEL_ERROR, "LOG_ERROR emits at LOG_LEVEL_ERROR");

  LOG_FATAL(&logger, "m", LOG_FIELD_INT32("x", 0));
  ASSERT_BOOL(p.level == LOG_LEVEL_FATAL, "LOG_FATAL emits at LOG_LEVEL_FATAL");

  /* No attributes: the same macro dispatches to the fieldless path. */
  p.calls = 0;
  LOG_INFO(&logger, "bare");
  ASSERT_INT_EQ(1, p.calls, "a LOG_<LEVEL> call with no attributes still emits");
  ASSERT_BOOL(p.field_count == 0, "a no-attribute call produces a fieldless record");
  ASSERT_BOOL(string_view_equals(p.message, string_view_from_cstr("bare")),
              "a no-attribute call still forwards the message");

  /* One attribute. */
  LOG_INFO(&logger, "one", LOG_FIELD_INT32("code", 7));
  ASSERT_BOOL(p.field_count == 1 && p.fields[0].as.i32 == 7 &&
              string_view_equals(p.fields[0].key, string_view_from_cstr("code")),
              "a single-attribute call packs and forwards the field");

  /* Two attributes: fills the array to LOG_MAX_FIELDS_PER_RECORD. */
  LOG_INFO(&logger, "pair", LOG_FIELD_INT32("a", 11), LOG_FIELD_INT32("b", 22));
  ASSERT_BOOL(p.field_count == 2 &&
              p.fields[0].as.i32 == 11 && p.fields[1].as.i32 == 22,
              "a two-attribute call packs and forwards both fields");
}
