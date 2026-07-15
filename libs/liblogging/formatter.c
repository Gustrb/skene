#define _POSIX_C_SOURCE 200809L

#include <liblogging/record.h>
#include <libstrview/string_view.h>
#include <liblogging/formatter.h>
#include <liblogging/writer.h>

#include <time.h>

PRIVATE int __formatter_console_formatter(const log_record_t *record, writer_t *writer);

PRIVATE int writer_time_format_rfc3339(writer_t *writer, timestamp_t ts);
PRIVATE int writer_write_level(writer_t *writer, log_level_t level);
PRIVATE int writer_write_string_view(writer_t *writer, string_view_t sv);
PRIVATE int writer_write_field_value(writer_t *writer, const log_field_t *f);

PRIVATE int writer_write_u32_pad(writer_t *writer, uint32_t value, uint8_t pad);

PRIVATE int writer_write_u64(writer_t *writer, uint64_t value);
PRIVATE int writer_write_i64(writer_t *writer, int64_t value);
PRIVATE int writer_write_f64(writer_t *writer, double value);
PRIVATE int writer_write_ptr(writer_t *writer, const void *ptr);

PUBLIC log_formatter_fn_t console_formatter(void)
{
  return __formatter_console_formatter;
}

PUBLIC timestamp_t time_now(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (timestamp_t)ts.tv_sec * 1000000000LL + (timestamp_t)ts.tv_nsec;
}

// TODO: error check(?)
#define __WRITE(writer, s) writer->write(writer, string_view_from_cstr(s))

PRIVATE int __formatter_console_formatter(const log_record_t *record, writer_t *writer)
{
  __WRITE(writer, "[");
  writer_time_format_rfc3339(writer, record->timestamp);
  __WRITE(writer, " ");
  writer_write_level(writer, record->level);
  __WRITE(writer, "]: \"");

  writer_write_string_view(writer, record->message);

  __WRITE(writer, "\"");
  if (record->field_count > 0)
  {
    __WRITE(writer, " (");
  }

  for (uint8_t i = 0; i < record->field_count; ++i)
  {
    if (i > 0)
    {
      __WRITE(writer, ", ");
    }

    writer_write_string_view(writer, record->fields[i].key);
    __WRITE(writer, "=");
    writer_write_field_value(writer, &record->fields[i]);
  }

  if (record->field_count > 0)
  {
    __WRITE(writer, ")");
  }

  __WRITE(writer, "\n");
  
  return 0;
}

PRIVATE int writer_time_format_rfc3339(writer_t *writer, timestamp_t timestamp)
{
  time_t seconds = timestamp / 1000000000LL;
  long nanos = timestamp % 1000000000LL;

  struct tm tm;
  gmtime_r(&seconds, &tm);

  writer_write_u32_pad(writer, tm.tm_year + 1900, 4);
  __WRITE(writer, "-");

  writer_write_u32_pad(writer, tm.tm_mon + 1, 2);
  __WRITE(writer, "-");

  writer_write_u32_pad(writer, tm.tm_mday, 2);
  __WRITE(writer, "T");

  writer_write_u32_pad(writer, tm.tm_hour, 2);
  __WRITE(writer, ":");

  writer_write_u32_pad(writer, tm.tm_min, 2);
  __WRITE(writer, ":");

  writer_write_u32_pad(writer, tm.tm_sec, 2);
  __WRITE(writer, ".");

  writer_write_u32_pad(writer, nanos, 9);
  __WRITE(writer, "Z");

  return 0;
}

PRIVATE int writer_write_u32_pad(writer_t *writer, uint32_t value, uint8_t width)
{
  char digits[10];
  size_t len = 0;

  do {
    digits[len++] = '0' + (value % 10);
    value /= 10;
  } while (value != 0);

  while (len < width)
  {
    __WRITE(writer, "0");
    width--;
  }

  while (len > 0)
  {
    len--;
    writer->write(writer, (string_view_t){ .length = 1, .addr = &digits[len]  });
  }

  return 0;
}

PRIVATE int writer_write_level(writer_t *writer, log_level_t level)
{
  switch (level)
  {
  case LOG_LEVEL_DEBUG: return __WRITE(writer, "DEBUG");
  case LOG_LEVEL_TRACE: return __WRITE(writer, "TRACE");
  case LOG_LEVEL_ERROR: return __WRITE(writer, "ERROR");
  case LOG_LEVEL_FATAL: return __WRITE(writer, "FATAL");
  case LOG_LEVEL_INFO:  return __WRITE(writer, "INFO");
  case LOG_LEVEL_WARN:  return __WRITE(writer, "WARN");
  }
  return 0;
}

PRIVATE int writer_write_string_view(writer_t *writer, string_view_t sv)
{
  return writer->write(writer, sv);
}

PRIVATE int writer_write_u64(writer_t *writer, uint64_t value)
{
  /* u64 max (18446744073709551615) is 20 digits. */
  char digits[20];
  size_t len = 0;

  do {
    digits[len++] = '0' + (value % 10);
    value /= 10;
  } while (value != 0);

  while (len > 0)
  {
    len--;
    writer->write(writer, (string_view_t){ .length = 1, .addr = &digits[len] });
  }

  return 0;
}

PRIVATE int writer_write_i64(writer_t *writer, int64_t value)
{
  uint64_t magnitude;

  if (value < 0)
  {
    __WRITE(writer, "-");
    /* Negate in unsigned space so INT64_MIN doesn't overflow. */
    magnitude = -(uint64_t)value;
  }
  else
  {
    magnitude = (uint64_t)value;
  }

  return writer_write_u64(writer, magnitude);
}

PRIVATE int writer_write_f64(writer_t *writer, double value)
{
  char buf[32];
  int n = snprintf(buf, sizeof(buf), "%g", value);
  if (n < 0)
  {
    return WRITER_ERR_FAILED_TO_WRITE;
  }
  if ((size_t)n >= sizeof(buf))
  {
    n = sizeof(buf) - 1;
  }

  return writer->write(writer, (string_view_t){ .length = (size_t)n, .addr = buf });
}

PRIVATE int writer_write_ptr(writer_t *writer, const void *ptr)
{
  __WRITE(writer, "0x");

  uintptr_t value = (uintptr_t)ptr;
  /* uintptr_t is at most 16 hex digits on x86_64. */
  char digits[16];
  size_t len = 0;

  do {
    uint8_t nibble = value & 0xF;
    digits[len++] = nibble < 10 ? ('0' + nibble) : ('a' + (nibble - 10));
    value >>= 4;
  } while (value != 0);

  while (len > 0)
  {
    len--;
    writer->write(writer, (string_view_t){ .length = 1, .addr = &digits[len] });
  }

  return 0;
}

PRIVATE int writer_write_field_value(writer_t *writer, const log_field_t *f)
{
  switch (f->type)
  {
    case LOG_FIELD_TYPE_STRING:  return writer_write_string_view(writer, f->as.string);
    case LOG_FIELD_TYPE_INT64:   return writer_write_i64(writer, f->as.i64);
    case LOG_FIELD_TYPE_INT32:   return writer_write_i64(writer, f->as.i32);
    case LOG_FIELD_TYPE_UINT64:  return writer_write_u64(writer, f->as.u64);
    case LOG_FIELD_TYPE_DOUBLE:  return writer_write_f64(writer, f->as.f64);
    case LOG_FIELD_TYPE_POINTER: return writer_write_ptr(writer, f->as.ptr);
  }

  return 0;
}

