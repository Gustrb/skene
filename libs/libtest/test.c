#include <common/common.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdio.h>

PUBLIC int test_fixture_load(const char *path, char **out, size_t *len)
{
  int fd = open(path, O_RDONLY);
  if (fd == -1)
  {
    return -1;
  }

  struct stat file_info;
  if (fstat(fd, &file_info) == -1)
  {
    close(fd);
    return -1;
  }

  *out = malloc(sizeof(char) * file_info.st_size);
  if (out == NULL)
  {
    close(fd);
    return -1;
  }

  long nread = read(fd, *out, file_info.st_size);
  if (nread != file_info.st_size)
  {
    close(fd);
    return -1;
  }

  *len = file_info.st_size;

  close(fd);
  return 0;
}

