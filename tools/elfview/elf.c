#include "./elf.h"

#include <string.h>

PUBLIC int elf64_parse_buffer(const char *buffer, size_t bufflen, elf64_file_t *file)
{
  if (bufflen < sizeof(file->header))
  {
    // TODO: add proper error codes
    return -1;
  }

  memcpy(&file->header, buffer, sizeof(file->header));

  if (file->header.e_shnum >= SKENE_ELF_MAX_SECTION_HEADERS)
  {
    // TODO: add proper error codes
    return -1;
  }

  file->section_headers_count = file->header.e_shnum;

  memcpy(&file->section_headers, buffer + file->header.e_shoff, file->section_headers_count);


  return 0;
}
