#ifndef SKENE_LOG_H
#define SKENE_LOG_H

#include <liblogging/handler.h>
#include <liblogging/record.h>
#include <liblogging/writer.h>
#include <liblogging/formatter.h>

#define LOG_MAX_HANDLERS 2

#define LOGGER_ERR_TOO_MANY_HANDLERS 1
#define LOG_ERR_TOO_MANY_FIELDS 2

typedef struct {
  log_handler_t *handlers[LOG_MAX_HANDLERS];
  size_t handler_count;
} logger_t;

PUBLIC logger_t log_logger_new(void);
PUBLIC int32_t log_logger_add_handler(logger_t *logger, log_handler_t *handler);
PUBLIC int32_t log_emit(logger_t *logger, log_level_t l, string_view_t message, const log_field_t *fields, size_t field_count);
PUBLIC int32_t log_logger_write(logger_t *l, const log_record_t *record);

/*
 * The LOG_<LEVEL> macros accept a message and zero or more log_field_t
 * attributes through one call:
 *
 *   LOG_INFO(&logger, "started");
 *   LOG_INFO(&logger, "request", LOG_FIELD_INT32("status", 200));
 *
 * An empty variadic tail is ill-formed under strict ISO C (-Wpedantic), and a
 * zero-length `log_field_t fields[] = {}` is likewise rejected, so we cannot
 * simply forward `...` to log_emit. Instead we dispatch on the argument count:
 * logger and message are always present, so __LOG_DISPATCH never receives an
 * empty tail, and __LOG_PICK selects the fieldless path (__LOG_0) or the
 * array-packing path (__LOG_N) by how many arguments arrived.
 */
#define __LOG_0(level, logger, message)                                        \
  log_emit(logger, level, string_view_from_cstr(message), NULL, 0)

#define __LOG_N(level, logger, message, ...)                                   \
  do {                                                                         \
    log_field_t __fields[] = {__VA_ARGS__};                                    \
    log_emit(logger, level, string_view_from_cstr(message), __fields,          \
             ARRAY_SIZE(__fields));                                            \
  } while (0)

/* Picks the 6th argument; the padding tail keeps __LOG_PICK's own `...`
 * non-empty even for the zero-attribute (shortest) call. */
#define __LOG_PICK(_1, _2, _3, _4, _5, NAME, ...) NAME
#define __LOG_DISPATCH(...)                                                    \
  __LOG_PICK(__VA_ARGS__, __LOG_N, __LOG_N, __LOG_0, _pad)(__VA_ARGS__)

#define LOG_TRACE(...) __LOG_DISPATCH(LOG_LEVEL_TRACE, __VA_ARGS__)
#define LOG_DEBUG(...) __LOG_DISPATCH(LOG_LEVEL_DEBUG, __VA_ARGS__)
#define LOG_INFO(...)  __LOG_DISPATCH(LOG_LEVEL_INFO, __VA_ARGS__)
#define LOG_WARN(...)  __LOG_DISPATCH(LOG_LEVEL_WARN, __VA_ARGS__)
#define LOG_ERROR(...) __LOG_DISPATCH(LOG_LEVEL_ERROR, __VA_ARGS__)
#define LOG_FATAL(...) __LOG_DISPATCH(LOG_LEVEL_FATAL, __VA_ARGS__)

#endif
