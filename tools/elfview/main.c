#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <libstrview/string_view.h>

#include "./elf.h"

typedef enum {
  CLI_ERROR_NO_ERROR = 0,
  CLI_ERROR_NOPATH,
  CLI_ERROR_FAILTOOPEN,
  CLI_ERROR_FAILTOSTAT,
  CLI_ERROR_EMPTYFILE,
  CLI_ERROR_FAILTOMAP,
} cli_error_t;

typedef struct {
	const char *file_path;
	uint64_t flags;
} cli_opts_t;

#define CLI_PRINT_HEADERS (0x1 << 0)
#define CLI_PRINT_SECTION_HEADERS (0x1 << 1)

typedef struct {
	char *ptr;
	size_t len;
	int fd;
} mmap_file_t;

typedef struct {
	const char **items;
	size_t length;
	size_t position;
} slice_t;


PRIVATE string_view_t print_header_flag = {.addr="h", .length=1};
PRIVATE string_view_t print_section_header_flag = {.addr="S", .length=1};

#define SLICE_ADVANCE(s) (s).position++
#define SLICE_LOOKUP(s) (s).items[(s).position]
#define SLICE_EMPTY(s) ((s).position >= (s).length)
#define SLICE_LENGTH(s) ((s).lenght - (s).position)

PRIVATE cli_error_t cli_parse_arguments(int argc, const char **argv, cli_opts_t *env);
PRIVATE cli_error_t cli_mmap_file(cli_opts_t *env, mmap_file_t *);
PRIVATE cli_error_t cli_unmap_file(mmap_file_t *);
PRIVATE void cli_pretty_print_elf64_file(cli_opts_t *env, elf64_file_t *file);
PRIVATE void cli_pretty_print_elf64_headers(cli_opts_t *env, const elf64_header_t *header);
PRIVATE void cli_pretty_print_elf64_section_headers(cli_opts_t *env, elf64_file_t *file);

int main(int argc, const char **argv)
{
	cli_opts_t env = {0};
	cli_error_t err = cli_parse_arguments(argc, argv, &env);
	if (err != CLI_ERROR_NO_ERROR)
	{
		// TODO: stringify the error, etc...
		return 1;
	}

	mmap_file_t file = {0};
	err = cli_mmap_file(&env, &file);
	if (err != CLI_ERROR_NO_ERROR)
	{
		return 1;
	}

	elf64_file_t elf_content = {0};
  elf64_error_t elf_err = elf64_parse_buffer(file.ptr, file.len, &elf_content);
  if (elf_err != ELF64_OK)
  {
  	fprintf(stderr, "failed to parse ELF file: %s\n", elf64_strerror(elf_err));
  	cli_unmap_file(&file);
  	return 1;
  }

  cli_pretty_print_elf64_file(&env, &elf_content);

	cli_unmap_file(&file);
	return 0;
}

PRIVATE void cli_pretty_print_elf64_file(cli_opts_t *env, elf64_file_t *file)
{
	if ((env->flags & CLI_PRINT_HEADERS) != 0)
	{
		cli_pretty_print_elf64_headers(env, file->header);
	}

	if ((env->flags & CLI_PRINT_SECTION_HEADERS) != 0)
	{
		cli_pretty_print_elf64_section_headers(env, file);
	}
}

#define CLI_NUM_HEADER_TYPES 5
PRIVATE const char *elf64_header_type_str[CLI_NUM_HEADER_TYPES] = {
	"None",
	"Rel",
	"Exec",
	"DYN (Position-Independent Executable file)",
	"CORE"
};

PRIVATE void cli_pretty_print_elf64_headers(cli_opts_t *env, const elf64_header_t *header)
{
	UNUSED(env);

	printf("ELF header:\n");

	printf("\tMagic number:\t");
	for (size_t i = SKENE_ELF_IDENT_MAG0; i < SKENE_ELF_MAGIC_SIZE; ++i)
	{
		printf("%x ", header->e_ident[i]);
	}

	// Note: since currently we only support a small range,
	// this code is filled with assertions, remove them as they stop making sense...
	assert(header->e_ident[SKENE_ELF_IDENT_CLASS] == SKENE_ELF_CLASS64);
	printf("\n\tClass: \tELF64\n");

	assert(header->e_ident[SKENE_ELF_DATA] == SKENE_ELF_DATA_2LSB);
	printf("\tData:\t");
	printf("2's complement, little endian\n");

	assert(header->e_ident[SKENE_ELF_VERSION] == 1);
	printf("\tVersion: \t1 (current)\n");

	assert(header->e_ident[SKENE_ELF_OSABI] == SKENE_ELF_OSABI_SYSV);
	printf("\tOS/ABI: \tUNIX - System V\n");

	assert(header->e_ident[SKENE_ELF_ABIVERSION] == 0);
	printf("\tABI Version:\t0\n");

	assert(header->e_type < CLI_NUM_HEADER_TYPES);
	printf("\tType:\t%s\n", elf64_header_type_str[header->e_type]);

	assert(header->e_machine == SKENE_ELF_MACHINE_AMD_X86_64);
	printf("\tMachine: \tAdvanced Micro Devices X86-64\n");

	printf("\tVersion: \t0x%x\n", header->e_version);
	printf("\tEntry point address: \t0x%lx\n", header->e_entry);
	printf("\tStart of program headers:\t%ld (bytes)\n", header->e_phoff);
	printf("\tStart of section headers:\t%ld (bytes)\n", header->e_shoff);
	printf("\tFlags:\t0x%x\n", header->e_flags);
	printf("\tSize of this header: \t%d (bytes)\n", header->e_ehsize);
	printf("\tSize of the program headers: \t%d (bytes)\n", header->e_phentsize);
	printf("\tNumber of program headers: \t%d\n", header->e_phnum);
	printf("\tSize of the section headers: \t%d (bytes)\n", header->e_shentsize);
	printf("\tNumber of section headers: \t%d\n", header->e_shnum);
	printf("\tIndex into the table of the section chain:\t %d\n", header->e_shstrndx);
}

PRIVATE void cli_pretty_print_elf64_section_headers(cli_opts_t *env, elf64_file_t *file)
{
	UNUSED(env);

	printf("There are %d section headers, starting at offset 0x%lx\n\n", file->header->e_shnum, file->header->e_shoff);
	printf("Section headers:\n");

	for (elf64_word_t i = 0; i < file->section_headers_count; ++i)
	{
		const elf64_section_header_t *header = &file->section_headers[i];
		printf("[%d] %s\n", i, elf64_section_name(file, header));
	}
}

uint8_t string_starts_with(const char *cstr, size_t len, const char *s)
{
	size_t slen = strlen(s);
	if (slen > len)
	{
		return 0;
	}

	// Note: probably really easy to SIMD here.
	for (size_t i = 0; i < slen; ++i)
	{
		if (cstr[i] != s[i])
		{
			return 0;
		}
	}

	return 1;
}

PRIVATE cli_error_t cli_parse_arguments(int argc, const char **argv, cli_opts_t *env)
{
	slice_t window = { .items = argv+1, .length = argc-1, .position=0};

	while (!SLICE_EMPTY(window))
	{	
		const char *v = SLICE_LOOKUP(window);
		size_t len = strlen(v);

		if (!string_starts_with(v, len, "-"))
		{
			env->file_path = v;
			SLICE_ADVANCE(window);
		}
		else
		{
			// We know it is a flag and it starts with '-'
			string_view_t sv = { .addr=v+1, .length=len-1 };

			if (string_view_equals(sv, print_header_flag))
			{
				env->flags |= CLI_PRINT_HEADERS;
				SLICE_ADVANCE(window);
			}
			else if (string_view_equals(sv, print_section_header_flag))
			{
				env->flags |= CLI_PRINT_SECTION_HEADERS;
				SLICE_ADVANCE(window);
			}
			else
			{
				TODO("please implement handling of other flags");
			}
		}
	}

	if (env->file_path == NULL)
	{
		return CLI_ERROR_NOPATH;
	}
	
	return CLI_ERROR_NO_ERROR;
}

PRIVATE cli_error_t cli_mmap_file(cli_opts_t *env, mmap_file_t *file)
{
	int file_descriptor = open(env->file_path, O_RDONLY);
	if (file_descriptor < 0)
	{
		return CLI_ERROR_FAILTOOPEN;
	}

	struct stat st;
	if (fstat(file_descriptor, &st) < 0)
	{
		return CLI_ERROR_FAILTOSTAT;
  }

  size_t lenght = (size_t)st.st_size;
  if (lenght == 0)
  {
  	close(file_descriptor);
  	return CLI_ERROR_EMPTYFILE;
  }

  void *addr = mmap(NULL, lenght, PROT_READ, MAP_PRIVATE, file_descriptor, 0);
  if (addr == MAP_FAILED)
  {
  	close(file_descriptor);
  	return CLI_ERROR_FAILTOMAP;
  }

	file->fd = file_descriptor;
	file->len = lenght;
	file->ptr = (char *)addr;

	return CLI_ERROR_NO_ERROR;
}

PRIVATE cli_error_t cli_unmap_file(mmap_file_t *file)
{
	munmap((void *)file->ptr, file->len);
	close(file->fd);

	file->fd = -1;
	file->len = 0;
	file->ptr = NULL;

	return CLI_ERROR_NO_ERROR;
}
