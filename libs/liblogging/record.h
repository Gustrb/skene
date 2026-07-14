#ifndef SKENE_LOG_RECORD_H
#define SKENE_LOG_RECORD_H

#include <common/common.h>
#include <libstrview/string_view.h>

typedef enum {
  LOG_LEVEL_TRACE,
  LOG_LEVEL_DEBUG,
  LOG_LEVEL_INFO,
  LOG_LEVEL_WARN,
  LOG_LEVEL_ERROR,
  LOG_LEVEL_FATAL,
} log_level_t;

typedef enum {
  LOG_FIELD_TYPE_STRING,
  LOG_FIELD_TYPE_INT64,
  LOG_FIELD_TYPE_INT32,
  LOG_FIELD_TYPE_UINT64,
  LOG_FIELD_TYPE_DOUBLE,
  LOG_FIELD_TYPE_POINTER,
} log_field_type_t;

typedef int64_t timestamp_t;

typedef struct {
  string_view_t key;
  log_field_type_t type;

  union {
    string_view_t string;
    int64_t i64;
    int32_t i32;
    uint64_t u64;
    double f64;
    const void *ptr;
  } as;
} log_field_t;

// Note: bump as necessary. I've chosen the smallest number possible to be annoying
#define LOG_MAX_FIELDS_PER_RECORD 2

typedef struct {
  timestamp_t timestamp;

  log_level_t level;
  string_view_t message;

  log_field_t fields[LOG_MAX_FIELDS_PER_RECORD];
  uint8_t field_count;
} log_record_t;

#define LOG_FIELD_STRING(k, v)  ((log_field_t){ .key=string_view_from_cstr(k), .type=LOG_FIELD_TYPE_STRING, .as.string = v                        })
#define LOG_FIELD_CSTRING(k, v) ((log_field_t){ .key=string_view_from_cstr(k), .type=LOG_FIELD_TYPE_STRING, .as.string = string_view_from_cstr(v) })
#define LOG_FIELD_INT64(k, v)   ((log_field_t){ .key=string_view_from_cstr(k), .type=LOG_FIELD_TYPE_INT64,  .as.i64    = v                        })
#define LOG_FIELD_INT32(k, v)   ((log_field_t){ .key=string_view_from_cstr(k), .type=LOG_FIELD_TYPE_INT32,  .as.i32    = v                        })
#define LOG_FIELD_ERR(k, v)     ((log_field_t){ .key=string_view_from_cstr(k), .type=LOG_FIELD_TYPE_INT32,  .as.i32    = v                        })
#define LOG_FIELD_UINT64(k, v)  ((log_field_t){ .key=string_view_from_cstr(k), .type=LOG_FIELD_TYPE_UINT64, .as.u64    = v                        })
#define LOG_FIELD_DOUBLE(k, v)  ((log_field_t){ .key=string_view_from_cstr(k), .type=LOG_FIELD_TYPE_DOUBLE, .as.f64    = v                        })
#define LOG_FIELD_PTR(k, v)     ((log_field_t){ .key=string_view_from_cstr(k), .type=LOG_FIELD_TYPE_POINTER, .as.ptr   = v                        })

#endif
