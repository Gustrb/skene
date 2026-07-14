#ifndef SKENE_WRITER_H
#define SKENE_WRITER_H

#include <common/common.h>
#include <libstrview/string_view.h>

typedef struct writer_t writer_t;

struct writer_t {
  union {
    FILE *file;
    int socket;
    void *ptr;
  } handle;

  int32_t (*write)(writer_t *, string_view_t sv);
  int32_t (*flush)(writer_t *);
};

#define WRITER_ERR_FAILED_TO_WRITE -1
#define WRITER_ERR_FAILED_TO_FLUSH -2

PUBLIC writer_t writer_get_console_writer(void);

#endif
