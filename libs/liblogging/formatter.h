#ifndef SKENE_LOG_FORMATTER_H
#define SKENE_LOG_FORMATTER_H

#include <liblogging/writer.h>
#include <liblogging/record.h>

typedef int (*log_formatter_fn_t)(const log_record_t *, writer_t *);

PUBLIC log_formatter_fn_t console_formatter(void);

#endif
