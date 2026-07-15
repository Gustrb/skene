#include <string.h>

#include <liblogging/log.h>

PUBLIC logger_t log_logger_new(void)
{
  return (logger_t){.handler_count=0};
}

PUBLIC int32_t log_logger_add_handler(logger_t *logger, log_handler_t *handler)
{
  if (logger->handler_count >= LOG_MAX_HANDLERS)
  {
    return LOGGER_ERR_TOO_MANY_HANDLERS;
  }

  logger->handlers[logger->handler_count++] = handler;

  return 0;
}


PUBLIC int32_t log_emit(logger_t *logger, log_level_t l, string_view_t message, const log_field_t *fields, size_t field_count)
{
  if (field_count > LOG_MAX_FIELDS_PER_RECORD)
  {
    return LOG_ERR_TOO_MANY_FIELDS;
  }

  log_record_t record = {
    .message = message,
    .timestamp = time_now(),
    .level = l,
    .field_count = field_count
  };

  /* fields may be NULL for a zero-attribute record; memcpy requires valid
   * pointers even when the count is 0, so guard the copy. */
  if (field_count > 0)
  {
    memcpy(record.fields, fields, field_count * sizeof(log_field_t));
  }

  return log_logger_write(logger, &record);
}

PUBLIC int32_t log_logger_write(logger_t *l, const log_record_t *record)
{
  int32_t err = 0;

  for (size_t i = 0; i < l->handler_count; ++i)
  {
    err = log_handler_write(l->handlers[i], record);
    if (err != 0)
    {
      return err;
    }
  }

  return err;
}
