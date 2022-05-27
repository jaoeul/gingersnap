#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include <stdint.h>

#include "program_header.h"

typedef enum {
        ELF_TYPE_NONE   = 0x0000,  // Unknown.
        ELF_TYPE_REL    = 0x0001,  // Relocatable file.
        ELF_TYPE_EXEC   = 0x0002,  // Executable file.
        ELF_TYPE_DYN    = 0x0003,  // Shared object.
        ELF_TYPE_CORE   = 0x0004,  // Core file.
        ELF_TYPE_LOOS   = 0xFE00,  // Reserved inclusive range. Operating system specific.
        ELF_TYPE_HIOS   = 0xFEFF,
        ELF_TYPE_LOPROC = 0xFF00,  // Reserved inclusive range. Processor specific.
        ELF_TYPE_HIPROC = 0xFFFF,
} enum_elf_type_t;

typedef enum {
    ELF_HEADER_FIELD_IDENT        = 0x00,
    ELF_HEADER_FIELD_EICLASS      = 0x04,
    ELF_HEADER_FIELD_EIDATA       = 0x05,
    ELF_HEADER_FIELD_TYPE         = 0x10,
    ELF_HEADER_FIELD_MACHINE      = 0x12,
    ELF_HEADER_FIELD_VERSION      = 0x14,
    ELF_HEADER_FIELD_ENTRY        = 0x18,
    ELF_HEADER_FIELD_PHOFF_32     = 0x1C,
    ELF_HEADER_FIELD_PHOFF_64     = 0x20,
    ELF_HEADER_FIELD_SHOFF_32     = 0x20,
    ELF_HEADER_FIELD_SHOFF_64     = 0x28,
    ELF_HEADER_FIELD_FLAGS_32     = 0x24,
    ELF_HEADER_FIELD_FLAGS_64     = 0x30,
    ELF_HEADER_FIELD_EHSIZE_32    = 0x28,
    ELF_HEADER_FIELD_EHSIZE_64    = 0x34,
    ELF_HEADER_FIELD_PHENTSIZE_32 = 0x2A,
    ELF_HEADER_FIELD_PHENTSIZE_64 = 0x36,
    ELF_HEADER_FIELD_PHNUM_32     = 0x2C,
    ELF_HEADER_FIELD_PHNUM_64     = 0x38,
    ELF_HEADER_FIELD_SHENTSIZE_32 = 0x2E,
    ELF_HEADER_FIELD_SHENTSIZE_64 = 0x3A,
    ELF_HEADER_FIELD_SHNUM_32     = 0x30,
    ELF_HEADER_FIELD_SHNUM_64     = 0x3C,
    ELF_HEADER_FIELD_SHSTRNDX_32  = 0x32,
    ELF_HEADER_FIELD_SHSTRNDX_64  = 0x3E,
} enum_elf_header_field_t;

typedef struct{
    char* path;
    enum_elf_type_t type;
	enum_bitsize_t bitsize;
	enum_endianess_t endianess;
	uint64_t entry_point;
	uint64_t nb_program_headers;
	program_header_t* program_headers;
	uint64_t program_header_offset;
	uint8_t* data;
	uint64_t data_length;
	uint64_t program_header_size;
} elf_t;

void
elf_print(const elf_t* elf);

elf_t*
elf_create(char* path);

void
elf_destroy(elf_t* elf);

#endif // ELF_LOADER_H
