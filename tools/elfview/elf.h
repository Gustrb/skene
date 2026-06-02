#ifndef SKENE_ELF_H
#define SKENE_ELF_H

// ELF file format support:
// This file presents all the necessary data structures and functions related to vieweing
// parsing, processing, etc. ELF files.
// As of now, we only support 64 bit variants, as the whole target of the project is ELF-based 64 bit systems
// References:
// https://uclibc.org/docs/elf-64-gen.pdf
// Linux kernel definitions: https://github.com/torvalds/linux/blob/16f73eb02d7e1765ccab3d2018e0bd98eb93d973/include/uapi/linux/elf.h#L1
// Linux insides (ELF format): https://0xax.gitbooks.io/linux-insides/content/Theory/linux-theory-2.html
// elfmaster library: https://github.com/elfmaster/libelfmaster/tree/master

#include <common/common.h>

//  e_ident identify the file as an ELF object file, and provide information
//  about the data representation of the object file structures. The bytes of this
//  array that have defined meanings are detailed below. The remaining bytes
//  are reserved for future use, and should be set to zero. Each byte of the
//  array is indexed symbolically using following names:
// 
// Purpose: file identification
#define SKENE_ELF_IDENT_MAG0 0
// Purpose: not set, should be 0d
#define SKENE_ELF_IDENT_MAG1 1
// Purpose: not set, should be 0d
#define SKENE_ELF_IDENT_MAG2 2
// Purpose: not set, should be 0d
#define SKENE_ELF_IDENT_MAG3 3
// Purpose: file class
#define SKENE_ELF_IDENT_CLASS 4
// Purpose: data encoding
#define SKENE_ELF_DATA 5
// Purpose: file version
#define SKENE_ELF_VERSION 6
// Purpose: OS/ABI identification
#define SKENE_ELF_OSABI 7
// Purpose: ABI version
#define SKENE_ELF_ABIVERSION 8
// Purpose: Padding bytes
#define SKENE_ELF_PAD 9
// Purpose: size of e_ident[]
#define SKENE_ELF_NIDENT 16

// e_ident[EI_MAG0] through e_ident[EI_MAG3] contain a “magic number,”
// identifying the file as an ELF object file. They contain the characters ‘\x7f’,
// ‘E’, ‘L’, and ‘F’, respectively.
// Magic number has 4 bytes and should be:
#define SKENE_ELF_MAGIC_SIZE 4
#define SKENE_ELF_MAGIC_WORD "\x7f" "ELF"

// e_ident[EI_CLASS] identifies the class of the object file, or its capacity.
// Table 3 lists the possible values.
// This document describes the structures for ELFCLASS64.
// The class of the ELF file is independent of the data model assumed by the
// object code. The EI_CLASS field identifies the file format;
// a processorspecific flag in the e_flags field, described below, may be used to identify
// the application’s data model if the processory supports multiple models.
//
// i.e. ELF file of a 32-bit binary
#define SKENE_ELF_CLASS32 1

// i.e. ELF file of a 64-bit binary
#define SKENE_ELF_CLASS64 2

// e_ident[EI_DATA] specifies the data encoding of the object file data
// structures. Table 4 lists the encodings defined for ELF-64.
// For the convenience of code that examines ELF object files at run time
// (e.g., the dynamic loader), it is intended that the data encoding of the
// object file will match that of the running program. For environments that
// support both byte orders, a processor-specific flag in the e_flags field,
// described below, may be used to identify the application’s operating mode.
//
// Object file data structures are littleendian
#define SKENE_ELF_DATA_2LSB 1
// Object file data structures are bigendian
#define SKENE_ELF_DATA_2MSB 2

// e_ident[EI_OSABI] identifies the operating system and ABI for which the
// object is prepared. Some fields in other ELF structures have flags and
// values that have environment-specific meanings; the interpretation of
// those fields is determined by the value of this field. Table 5 lists the
// currently-defined values for this field.
//
// i.e. System V ABI
#define SKENE_ELF_OSABI_SYSV 0
// i.e. HP-UX operating system
#define SKENE_ELF_OSABI_HPUX 1
// i.e. Standalone (embedded) application
#define SKENE_ELF_OSABI_STANDALONE 255

// e_type identifies the object file type.
//
// i.e. No file type
#define SKENE_ELF_TYPE_NONE 0
// i.e. Relocatable object file
#define SKENE_ELF_TYPE_REL 1
// i.e. Executable file
#define SKENE_ELF_TYPE_EXEC 2
// i.e. Shared object file
#define SKENE_ELF_TYPE_DYN 3
// i.e. Core file
#define SKENE_ELF_TYPE_CORE 4
// i.e. Environment-specific use
#define SKENE_ELF_TYPE_LOOS 0xFE00
// i.e. ...
#define SKENE_ELF_TYPE_HIOS 0xFEFF
// i.e. Processor-specific use
#define SKENE_ELF_TYPE_LOPROC 0xFF00
// i.e. ...
#define SKENE_ELF_TYPE_HIPROC 0xFFFF

// List of machines https://gist.github.com/x0nu11byt3/bcb35c3de461e5fb66173071a2379779
#define SKENE_ELF_MACHINE_AMD_X86_64 62

typedef uint16_t elf64_half_t;
typedef uint32_t elf64_word_t;
typedef uint64_t elf64_addr_t;
typedef uint64_t elf64_off_t;
typedef uint64_t elf64_xword_t;

typedef struct {
  unsigned char e_ident[SKENE_ELF_NIDENT]; /* ELF identifications */
  elf64_half_t e_type;                        /* Object file type */
  elf64_half_t e_machine;                     /* Machine type */
  elf64_word_t e_version;                     /* Object file version */
  elf64_addr_t e_entry;                       /* Entry point address */
  elf64_off_t e_phoff;                        /* Program header offset */
  elf64_off_t e_shoff;                        /* Section header offset */
  elf64_word_t e_flags;                       /* Processor-specific flags */
  elf64_half_t e_ehsize;                      /* ELF header size */
  elf64_half_t e_phentsize;                   /* Size of program header entry */
  elf64_half_t e_phnum;     /* Number of program header entries */
  elf64_half_t e_shentsize; /* Size of section header entry */
  elf64_half_t e_shnum;     /* Number of section header entries */
  elf64_half_t e_shstrndx;  /* Section name string table index */
} elf64_header_t;

typedef struct {
  elf64_word_t sh_name;       /* Section name */
  elf64_word_t sh_type;       /* Section type */
  elf64_xword_t sh_flags;     /* Section attributes */
  elf64_addr_t sh_addr;       /* Virtual address in memory */
  elf64_off_t sh_offset;      /* Offset in file */
  elf64_xword_t sh_size;      /* Size of section */
  elf64_word_t sh_link;       /* Link to other section */
  elf64_word_t sh_info;       /* Miscellaneous information */
  elf64_xword_t sh_addralign; /* Address alignment boundary */
  elf64_xword_t sh_entsize;   /* Size of entries, if section has table */
} elf64_section_header_t;

// Marks an unused section header
#define SKENE_ELF_SECTION_HEADER_TYPE_NULL 0
// Contains information defined by the program
#define SKENE_ELF_SECTION_HEADER_TYPE_PROGBITS 1
// Contains a linker symbol table
#define SKENE_ELF_SECTION_HEADER_TYPE_SYMTAB 2
// Contains a string table
#define SKENE_ELF_SECTION_HEADER_TYPE_STRTAB 3
// Contains “Rela” type relocation entries
#define SKENE_ELF_SECTION_HEADER_TYPE_RELA 4
// Contains a symbol hash table
#define SKENE_ELF_SECTION_HEADER_TYPE_HASH 5
// Contains dynamic linking tables
#define SKENE_ELF_SECTION_HEADER_TYPE_DYNAMIC 6
// Contains note information
#define SKENE_ELF_SECTION_HEADER_TYPE_NOTE 7
// Contains uninitialized space; does not occupy any space in the file
#define SKENE_ELF_SECTION_HEADER_TYPE_NOBITS 8
// Contains “Rel” type relocation entries
#define SKENE_ELF_SECTION_HEADER_TYPE_REL 9
// Reserved
#define SKENE_ELF_SECTION_HEADER_TYPE_SHLIB 10
// Contains a dynamic loader symbol table
#define SKENE_ELF_SECTION_HEADER_TYPE_DYNSYM 11
// Environment-specific use
#define SKENE_ELF_SECTION_HEADER_TYPE_LOOS 0x60000000
#define SKENE_ELF_SECTION_HEADER_TYPE_HIOS 0x6FFFFFFF

// Processor-specific use
#define SKENE_ELF_SECTION_HEADER_TYPE_LOPROC 0x70000000
#define SKENE_ELF_SECTION_HEADER_TYPE_HIPROC 0x7FFFFFFF

// Section contains writable data
#define SKENE_ELF_SECTION_HEADER_FLAG_SHF_WRITE 0x1
// Section is allocated in memory image of program
#define SKENE_ELF_SECTION_HEADER_FLAG_SHF_ALLOC 0x2
// Section contains executable instructions
#define SKENE_ELF_SECTION_HEADER_FLAG_SHF_EXECINSTR 0x4
// Environment-specific use
#define SKENE_ELF_SECTION_HEADER_FLAG_SHF_MASKOS 0x0F000000
// Processor-specific use
#define SKENE_ELF_SECTION_HEADER_FLAG_SHF_MASKPROC 0xF0000000

typedef struct {
  // type of segment
  elf64_word_t p_type;
  // segment attributes
  elf64_word_t p_flags;
  // offset in file
  elf64_off_t p_offset;
  // virtual address in memory
  elf64_addr_t p_vaddr;
  // reserved
  elf64_addr_t p_paddr;
  // size of segument in file
  elf64_xword_t p_filesz;
  // size of segment in memory
  elf64_xword_t p_memsz;
  // alignment of segment
  elf64_xword_t p_align;
} elf64_program_header_t;

#define SKENE_ELF_PROGRAM_HEADER_TYPE_NULL 0
#define SKENE_ELF_PROGRAM_HEADER_TYPE_LOAD 1
#define SKENE_ELF_PROGRAM_HEADER_TYPE_DYNAMIC 2
#define SKENE_ELF_PROGRAM_HEADER_TYPE_INTERP 3
#define SKENE_ELF_PROGRAM_HEADER_TYPE_NOTE 4
#define SKENE_ELF_PROGRAM_HEADER_TYPE_SHLIB 5
#define SKENE_ELF_PROGRAM_HEADER_TYPE_PHDR 6
#define SKENE_ELF_PROGRAM_HEADER_TYPE_LOOS 0x60000000
#define SKENE_ELF_PROGRAM_HEADER_TYPE_HIOS 0x6FFFFFFF
#define SKENE_ELF_PROGRAM_HEADER_TYPE_LOPROC 0x70000000
#define SKENE_ELF_PROGRAM_HEADER_TYPE_HIPROC 0x7FFFFFFF


#define SKENE_ELF_MAX_SECTION_HEADERS 65535

// elf64_file_t is a *zero-copy view* over a buffer (typically an mmap'd file).
// None of the pointers below are owned: they point directly into `base`, which
// must outlive the elf64_file_t. There is nothing to free.
typedef struct {
  const char *base; /* The backing buffer the file was parsed from */
  size_t len;       /* Size of the backing buffer, in bytes */

  const elf64_header_t *header; /* -> base + 0 */

  const elf64_section_header_t *section_headers; /* -> base + header->e_shoff */
  elf64_half_t section_headers_count;

  const elf64_program_header_t *program_headers;
  elf64_half_t program_headers_count;

  const char *shstrtab;  /* Section-header string table -> base + sh_offset */
  size_t shstrtab_size;  /* Size of the string table, in bytes */
} elf64_file_t;

typedef enum {
  ELF64_OK = 0,
  // The buffer is smaller than the ELF header.
  ELF64_ERR_TRUNCATED,
  // header->e_shnum is at/above SKENE_ELF_MAX_SECTION_HEADERS.
  ELF64_ERR_TOO_MANY_SECTIONS,
  // header->e_shentsize does not match sizeof(elf64_section_header_t).
  ELF64_ERR_BAD_SHENTSIZE,
  // The section header table extends past the end of the buffer.
  ELF64_ERR_SECTIONS_OUT_OF_BOUNDS,
  // header->e_shstrndx does not point at a valid section header.
  ELF64_ERR_BAD_SHSTRNDX,
  // The section-header string table extends past the end of the buffer.
  ELF64_ERR_SHSTRTAB_OUT_OF_BOUNDS,
} elf64_error_t;

// Parses `buffer` (which must remain valid for the lifetime of *file) into a
// zero-copy view. Returns ELF64_OK on success or an elf64_error_t on failure.
PUBLIC elf64_error_t elf64_parse_buffer(const char *buffer, size_t bufflen, elf64_file_t *file);

// Returns a human-readable, static string for an elf64_error_t.
PUBLIC const char *elf64_strerror(elf64_error_t err);

// Returns the name of `section` as a NUL-terminated string into the string
// table, or "" if the name offset is out of bounds.
PUBLIC const char *elf64_section_name(const elf64_file_t *file, const elf64_section_header_t *section);

#endif
