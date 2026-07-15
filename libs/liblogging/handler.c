#include <liblogging/handler.h>

PUBLIC int32_t log_handler_write(log_handler_t *handler, const log_record_t *record)
{
  handler->formatter(record, handler->writer);
  return handler->writer->flush(handler->writer);
}

PUBLIC log_handler_t log_handler_new(writer_t *w, log_formatter_fn_t f)
{
  return (log_handler_t){.formatter=f, .writer=w};
}
