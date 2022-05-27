#ifndef PROGRAM_HEADER_H
#define PROGRAM_HEADER_H

#include "../utils/endianess_converter.h"

typedef enum {
    PROGRAM_HEADER_TYPE_NULL    = 0x00000000, // Program header table entry unused.
    PROGRAM_HEADER_TYPE_LOAD    = 0x00000001, // Loadable segment.
    PROGRAM_HEADER_TYPE_DYNAMIC = 0x00000002, // Dynamic linking information.
    PROGRAM_HEADER_TYPE_INTERP  = 0x00000003, // Interpreter information.
    PROGRAM_HEADER_TYPE_NOTE    = 0x00000004, // Auxiliary information.
    PROGRAM_HEADER_TYPE_SHLIB   = 0x00000005, // Reserved.
    PROGRAM_HEADER_TYPE_PHDR    = 0x00000006, // Segment containing program header table itself.
    PROGRAM_HEADER_TYPE_TLS     = 0x00000007, // Thread-Local Storage template.
    PROGRAM_HEADER_TYPE_LOOS    = 0x60000000, // Reserved inclusive range. Operating system specific.
    PROGRAM_HEADER_TYPE_HIOS    = 0x6FFFFFFF, // Same as above.
    PROGRAM_HEADER_TYPE_LOPROC  = 0x70000000, // Reserved inclusive range. Processor specific.
    PROGRAM_HEADER_TYPE_HIPROC  = 0x7FFFFFFF, // Same as above.
} enum_program_pt_t;

typedef enum {
    PROGRAM_HEADER_FIELD_TYPE      = 0x00,
    PROGRAM_HEADER_FIELD_OFFSET_32 = 0x04,
    PROGRAM_HEADER_FIELD_OFFSET_64 = 0x08,
    PROGRAM_HEADER_FIELD_VADDR_32  = 0x08,
    PROGRAM_HEADER_FIELD_VADDR_64  = 0x10,
    PROGRAM_HEADER_FIELD_PADDR_32  = 0x0C,
    PROGRAM_HEADER_FIELD_PADDR_64  = 0x18,
    PROGRAM_HEADER_FIELD_FILESZ_32 = 0x10,
    PROGRAM_HEADER_FIELD_FILESZ_64 = 0x20,
    PROGRAM_HEADER_FIELD_MEMSZ_32  = 0x14,
    PROGRAM_HEADER_FIELD_MEMSZ_64  = 0x28,
    PROGRAM_HEADER_FIELD_ALIGN_32  = 0x1C,
    PROGRAM_HEADER_FIELD_ALIGN_64  = 0x30,
    PROGRAM_HEADER_FIELD_FLAGS_32  = 0x18,
    PROGRAM_HEADER_FIELD_FLAGS_64  = 0x04,
} enum_program_header_field_t;

typedef struct {
	uint64_t offset;
	uint64_t virtual_address;
	uint64_t physical_address;
	uint64_t file_size;
	uint64_t memory_size;
	uint64_t align;
	uint64_t flags;
} program_header_t;

uint64_t
program_header_parse_offset(uint8_t* prog_hdr_bytes, enum_bitsize_t bitsize, enum_endianess_t endianess);

uint64_t
program_header_parse_virtual_address(uint8_t* prog_hdr_bytes, enum_bitsize_t bitsize, enum_endianess_t endianess);

uint64_t
program_header_parse_physical_address(uint8_t* prog_hdr_bytes, enum_bitsize_t bitsize, enum_endianess_t endianess);

uint64_t
program_header_parse_file_size(uint8_t* prog_hdr_bytes, enum_bitsize_t bitsize, enum_endianess_t endianess);

uint64_t
program_header_parse_memory_size(uint8_t* prog_hdr_bytes, enum_bitsize_t bitsize, enum_endianess_t endianess);

uint64_t
program_header_parse_align(uint8_t* prog_hdr_bytes, enum_bitsize_t bitsize, enum_endianess_t endianess);

uint64_t
program_header_parse_flags(uint8_t* prog_hdr_bytes, enum_bitsize_t bitsize, enum_endianess_t endianess);

#endif
