#include "elf.h"
#include <libtest/test.h>
#include <string.h>

void __assert_elf64_magic_number(elf64_file_t *file);
void __assert_elf64_padding_is_0(elf64_file_t *file);
void __assert_elf64_class_is_correct(elf64_file_t *file);
void __assert_elf64_is_little_endian(elf64_file_t *file);
void __assert_elf64_abi_should_be_systemv(elf64_file_t *file);
void __assert_elf64_type_should_be_executable(elf64_file_t *file);
void __assert_elf64_section_header_num_should_be_correct(elf64_file_t *file);
void __assert_elf64_section_header_data(elf64_file_t *file);
void __assert_elf64_view_metadata(elf64_file_t *file, const char *base, size_t len);
void __assert_elf64_section_names(elf64_file_t *file);
void __assert_elf64_rejects_truncated_buffer(const char *fixture);

int main(void)
{
  TEST_START(50);
  char *fixture = NULL;
  size_t len = 0;

  int err = test_fixture_load("./tools/elfview/fixtures/hello-world", &fixture, &len);
  ASSERT_INT_EQ(0, err, "should have any error loading fixture");

  elf64_file_t file = {0};
  err = elf64_parse_buffer(fixture, len, &file);
  ASSERT_INT_EQ(ELF64_OK, err, "should not fail parsing elf file");

  __assert_elf64_magic_number(&file);
  __assert_elf64_padding_is_0(&file);
  __assert_elf64_class_is_correct(&file);
  __assert_elf64_is_little_endian(&file);
  __assert_elf64_abi_should_be_systemv(&file);
  __assert_elf64_type_should_be_executable(&file);
  __assert_elf64_section_header_num_should_be_correct(&file);
  __assert_elf64_section_header_data(&file);
  __assert_elf64_view_metadata(&file, fixture, len);
  __assert_elf64_section_names(&file);
  __assert_elf64_rejects_truncated_buffer(fixture);

  // The view is zero-copy: nothing in `file` is owned, so only the backing
  // buffer is freed.
  free(fixture);
  TEST_FINISH();
  return 0;
}

void __assert_elf64_magic_number(elf64_file_t *file)
{
  const char *magic = SKENE_ELF_MAGIC_WORD;
  for (int i = 0; i < SKENE_ELF_MAGIC_SIZE; ++i)
  {
    ASSERT_CHAR_EQ(file->header->e_ident[SKENE_ELF_IDENT_MAG0 + i], magic[i], "should match all magic bytes"); 
  }
}

void __assert_elf64_padding_is_0(elf64_file_t *file)
{
  ASSERT_INT_EQ(file->header->e_ident[SKENE_ELF_PAD], 0, "padding bytes should be zeroed out");
}

void __assert_elf64_class_is_correct(elf64_file_t *file)
{
  ASSERT_INT_EQ(SKENE_ELF_CLASS64, file->header->e_ident[SKENE_ELF_IDENT_CLASS], "class should always be 64");
}

void __assert_elf64_is_little_endian(elf64_file_t *file)
{
  ASSERT_INT_EQ(SKENE_ELF_DATA_2LSB, file->header->e_ident[SKENE_ELF_DATA], "should be little-endian on the supported platforms");
}

void __assert_elf64_abi_should_be_systemv(elf64_file_t *file)
{
  ASSERT_INT_EQ(SKENE_ELF_OSABI_SYSV, file->header->e_ident[SKENE_ELF_OSABI], "should have systemv abi");
}

void __assert_elf64_type_should_be_executable(elf64_file_t *file)
{
  ASSERT_INT_EQ(SKENE_ELF_TYPE_DYN, file->header->e_type, "should be type executable");
}

#define NUM_SECTION_HEADERS 31

void __assert_elf64_section_header_num_should_be_correct(elf64_file_t *file)
{
  ASSERT_INT_EQ(NUM_SECTION_HEADERS, file->section_headers_count, "there should only be a few headers");
}

void __assert_elf64_section_header_data(elf64_file_t *file)
{
  elf64_word_t section_header_types[NUM_SECTION_HEADERS] = {
    SKENE_ELF_SECTION_HEADER_TYPE_NULL,
    SKENE_ELF_SECTION_HEADER_TYPE_NOTE,
    SKENE_ELF_SECTION_HEADER_TYPE_NOTE,
    SKENE_ELF_SECTION_HEADER_TYPE_PROGBITS,
    /*SKENE_ELF_SECTION_HEADER_TYPE_HIOS*/ 1879048182,
    SKENE_ELF_SECTION_HEADER_TYPE_DYNSYM,
    SKENE_ELF_SECTION_HEADER_TYPE_STRTAB,
    /*SKENE_ELF_SECTION_HEADER_TYPE_HIOS*/ 1879048191,
    /*SKENE_ELF_SECTION_HEADER_TYPE_HIOS*/ 1879048190,
    SKENE_ELF_SECTION_HEADER_TYPE_RELA,
    SKENE_ELF_SECTION_HEADER_TYPE_RELA,
    SKENE_ELF_SECTION_HEADER_TYPE_PROGBITS,
    SKENE_ELF_SECTION_HEADER_TYPE_PROGBITS,
    SKENE_ELF_SECTION_HEADER_TYPE_PROGBITS,
    SKENE_ELF_SECTION_HEADER_TYPE_PROGBITS,
    SKENE_ELF_SECTION_HEADER_TYPE_PROGBITS,
    SKENE_ELF_SECTION_HEADER_TYPE_PROGBITS,
    SKENE_ELF_SECTION_HEADER_TYPE_PROGBITS,
    SKENE_ELF_SECTION_HEADER_TYPE_PROGBITS,
    SKENE_ELF_SECTION_HEADER_TYPE_NOTE,
    /* 14 */ 14,
    /* 14 */ 15,
    SKENE_ELF_SECTION_HEADER_TYPE_DYNAMIC,
    SKENE_ELF_SECTION_HEADER_TYPE_PROGBITS,
    SKENE_ELF_SECTION_HEADER_TYPE_PROGBITS,
    SKENE_ELF_SECTION_HEADER_TYPE_PROGBITS,
    SKENE_ELF_SECTION_HEADER_TYPE_NOBITS,
    SKENE_ELF_SECTION_HEADER_TYPE_PROGBITS,
    SKENE_ELF_SECTION_HEADER_TYPE_SYMTAB,
    SKENE_ELF_SECTION_HEADER_TYPE_STRTAB,
    SKENE_ELF_SECTION_HEADER_TYPE_STRTAB,
  };

  for (elf64_word_t i = 0; i < file->section_headers_count; ++i)
  {
    ASSERT_INT_EQ(section_header_types[i], file->section_headers[i].sh_type, "section header type should match");
  }
}

void __assert_elf64_view_metadata(elf64_file_t *file, const char *base, size_t len)
{
  // The view should point straight at the buffer it was parsed from.
  ASSERT_BOOL(file->base == base, "base should point at the backing buffer");
  ASSERT_BOOL(file->len == len, "len should match the backing buffer size");
  ASSERT_BOOL(file->shstrtab_size > 0, "string table should be non-empty");
}

void __assert_elf64_section_names(elf64_file_t *file)
{
  // The NULL section (index 0) has sh_name 0, which indexes the leading NUL of
  // the string table, i.e. the empty string.
  ASSERT_BOOL(strcmp(elf64_section_name(file, &file->section_headers[0]), "") == 0,
              "the null section should have an empty name");

  int found_text = 0;
  int found_shstrtab = 0;
  for (elf64_word_t i = 0; i < file->section_headers_count; ++i)
  {
    const char *name = elf64_section_name(file, &file->section_headers[i]);
    if (strcmp(name, ".text") == 0)
    {
      found_text = 1;
    }
    if (strcmp(name, ".shstrtab") == 0)
    {
      found_shstrtab = 1;
    }
  }

  ASSERT_BOOL(found_text, "should resolve the .text section name");
  ASSERT_BOOL(found_shstrtab, "should resolve the .shstrtab section name");
}

void __assert_elf64_rejects_truncated_buffer(const char *fixture)
{
  // A buffer smaller than the ELF header must be rejected, not parsed.
  elf64_file_t file = {0};
  elf64_error_t err = elf64_parse_buffer(fixture, sizeof(elf64_header_t) - 1, &file);
  ASSERT_INT_EQ(ELF64_ERR_TRUNCATED, err, "should reject a truncated buffer");
}
