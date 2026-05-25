#include "./elf.h"

#include <string.h>
#include <assert.h>

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

  // Note: validating struct layout has the correct structure
  // This is an assertion since we don't validate it and just slurp the memory into the
  // memory layout.
  assert(sizeof(file->section_headers[0]) == file->header.e_shentsize);

  file->section_headers_count = file->header.e_shnum;
  memcpy(&file->section_headers, buffer + file->header.e_shoff, file->section_headers_count * sizeof(file->section_headers[0]));

  elf64_section_header_t *shstrtab_hdr = &file->section_headers[file->header.e_shstrndx];
  file->shstrtab = malloc(shstrtab_hdr->sh_size);

  memcpy(file->shstrtab, buffer + shstrtab_hdr->sh_offset, shstrtab_hdr->sh_size);

  return 0;
}
