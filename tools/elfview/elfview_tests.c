#include "elf.h"
#include <libtest/test.h>

void __assert_elf64_magic_number(elf64_file_t *file);
void __assert_elf64_padding_is_0(elf64_file_t *file);
void __assert_elf64_class_is_correct(elf64_file_t *file);
void __assert_elf64_is_little_endian(elf64_file_t *file);
void __assert_elf64_abi_should_be_systemv(elf64_file_t *file);
void __assert_elf64_type_should_be_executable(elf64_file_t *file);
void __assert_elf64_section_header_num_should_be_correct(elf64_file_t *file);

int main(void)
{
  TEST_START(12);
  char *fixture = NULL;
  size_t len = 0;

  int err = test_fixture_load("./tools/elfview/fixtures/hello-world", &fixture, &len);
  ASSERT_INT_EQ(0, err, "should have any error loading fixture");

  elf64_file_t file = {0};
  err = elf64_parse_buffer(fixture, len, &file);
  ASSERT_INT_EQ(0, err, "should not fail parsing elf file");

  __assert_elf64_magic_number(&file);
  __assert_elf64_padding_is_0(&file);
  __assert_elf64_class_is_correct(&file);
  __assert_elf64_is_little_endian(&file);
  __assert_elf64_abi_should_be_systemv(&file);
  __assert_elf64_type_should_be_executable(&file);
  __assert_elf64_section_header_num_should_be_correct(&file);

  free(fixture);
  TEST_FINISH();
  return 0;
}

void __assert_elf64_magic_number(elf64_file_t *file)
{
  const char *magic = SKENE_ELF_MAGIC_WORD;
  for (int i = 0; i < SKENE_ELF_MAGIC_SIZE; ++i)
  {
    ASSERT_CHAR_EQ(file->header.e_ident[SKENE_ELF_IDENT_MAG0 + i], magic[i], "should match all magic bytes"); 
  }
}

void __assert_elf64_padding_is_0(elf64_file_t *file)
{
  ASSERT_INT_EQ(file->header.e_ident[SKENE_ELF_PAD], 0, "padding bytes should be zeroed out");
}

void __assert_elf64_class_is_correct(elf64_file_t *file)
{
  ASSERT_INT_EQ(SKENE_ELF_CLASS64, file->header.e_ident[SKENE_ELF_IDENT_CLASS], "class should always be 64");
}

void __assert_elf64_is_little_endian(elf64_file_t *file)
{
  ASSERT_INT_EQ(SKENE_ELF_DATA_2LSB, file->header.e_ident[SKENE_ELF_DATA], "should be little-endian on the supported platforms");
}

void __assert_elf64_abi_should_be_systemv(elf64_file_t *file)
{
  ASSERT_INT_EQ(SKENE_ELF_OSABI_SYSV, file->header.e_ident[SKENE_ELF_OSABI], "should have systemv abi");
}

void __assert_elf64_type_should_be_executable(elf64_file_t *file)
{
  ASSERT_INT_EQ(SKENE_ELF_TYPE_DYN, file->header.e_type, "should be type executable");
}

void __assert_elf64_section_header_num_should_be_correct(elf64_file_t *file)
{
  ASSERT_INT_EQ(31, file->section_headers_count, "there should only be a few headers");
}
