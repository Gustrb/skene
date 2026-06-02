#include "./elf.h"

#include <string.h>

// Returns 1 if [offset, offset + size) fits entirely within a buffer of
// `bufflen` bytes, guarding against overflow in `offset + size`.
PRIVATE int range_in_bounds(size_t offset, size_t size, size_t bufflen)
{
  if (offset > bufflen)
  {
    return 0;
  }

  // offset <= bufflen here, so (bufflen - offset) can't underflow.
  return size <= bufflen - offset;
}

PUBLIC elf64_error_t elf64_parse_buffer(const char *buffer, size_t bufflen, elf64_file_t *file)
{
  if (bufflen < sizeof(elf64_header_t))
  {
    return ELF64_ERR_TRUNCATED;
  }

  file->base = buffer;
  file->len = bufflen;
  file->header = (const elf64_header_t *)buffer;

  if (file->header->e_shnum >= SKENE_ELF_MAX_SECTION_HEADERS)
  {
    return ELF64_ERR_TOO_MANY_SECTIONS;
  }

  // We treat the section header table as an array of elf64_section_header_t laid
  // out exactly as in the file, so the on-disk entry size must match ours.
  if (file->header->e_shentsize != sizeof(elf64_section_header_t))
  {
    return ELF64_ERR_BAD_SHENTSIZE;
  }

  file->section_headers_count = file->header->e_shnum;

  size_t sections_size = (size_t)file->section_headers_count * sizeof(elf64_section_header_t);
  if (!range_in_bounds(file->header->e_shoff, sections_size, bufflen))
  {
    return ELF64_ERR_SECTIONS_OUT_OF_BOUNDS;
  }

  file->section_headers = (const elf64_section_header_t *)(buffer + file->header->e_shoff);

  if (file->header->e_shstrndx >= file->section_headers_count)
  {
    return ELF64_ERR_BAD_SHSTRNDX;
  }

  const elf64_section_header_t *shstrtab_hdr = &file->section_headers[file->header->e_shstrndx];
  if (!range_in_bounds(shstrtab_hdr->sh_offset, shstrtab_hdr->sh_size, bufflen))
  {
    return ELF64_ERR_SHSTRTAB_OUT_OF_BOUNDS;
  }

  file->shstrtab = buffer + shstrtab_hdr->sh_offset;
  file->shstrtab_size = shstrtab_hdr->sh_size;

  return ELF64_OK;
}

PUBLIC const char *elf64_strerror(elf64_error_t err)
{
  switch (err)
  {
  case ELF64_OK:
    return "no error";
  case ELF64_ERR_TRUNCATED:
    return "buffer is smaller than the ELF header";
  case ELF64_ERR_TOO_MANY_SECTIONS:
    return "too many section headers";
  case ELF64_ERR_BAD_SHENTSIZE:
    return "unexpected section header entry size";
  case ELF64_ERR_SECTIONS_OUT_OF_BOUNDS:
    return "section header table is out of bounds";
  case ELF64_ERR_BAD_SHSTRNDX:
    return "section name string table index is invalid";
  case ELF64_ERR_SHSTRTAB_OUT_OF_BOUNDS:
    return "section name string table is out of bounds";
  }

  return "unknown error";
}

PUBLIC const char *elf64_section_name(const elf64_file_t *file, const elf64_section_header_t *section)
{
  if (section->sh_name >= file->shstrtab_size)
  {
    return "";
  }

  return file->shstrtab + section->sh_name;
}
