#ifndef SKENE_HANDLER_H
#define SKENE_HANDLER_H

#include "liblogging/record.h"
#include <common/common.h>
#include <liblogging/writer.h>
#include <liblogging/formatter.h>


typedef struct log_handler_t {
  writer_t *writer;
  log_formatter_fn_t formatter;
} log_handler_t;

PUBLIC log_handler_t log_handler_new(writer_t *w, log_formatter_fn_t f);
PUBLIC int32_t log_handler_write(log_handler_t *handler, const log_record_t *record);

#endif
