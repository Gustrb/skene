#include <liblogging/writer.h>


PRIVATE int32_t console_writer_write(writer_t *, string_view_t sv);
PRIVATE int32_t console_writer_flush(writer_t *);

PUBLIC writer_t writer_get_console_writer(void)
{
  return (writer_t){
    .handle.file = stdout,
    .write = console_writer_write,
    .flush = console_writer_flush,
  };
}

PRIVATE int32_t console_writer_write(writer_t *w, string_view_t sv)
{
  uint32_t nbytes = fwrite(sv.addr, sizeof(char), sv.length, w->handle.file);
  if (nbytes == sv.length)
  {
    return 0;  
  }

  return WRITER_ERR_FAILED_TO_WRITE;
}

PRIVATE int32_t console_writer_flush(writer_t *w)
{
  int32_t err = fflush(w->handle.file);
  if (err == 0)
  {
    return 0;    
  }

  return WRITER_ERR_FAILED_TO_FLUSH;
}

