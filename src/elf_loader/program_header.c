#include <stdio.h>
#include <stdlib.h>

#include "program_header.h"

static enum_program_header_type_t
program_header_parse_type(uint8_t* prog_hdr_bytes, enum_endianess_t endianess)
{
    enum_program_header_type_t type = byte_arr_to_u64(prog_hdr_bytes, 4, ENUM_ENDIANESS_MSB);
    switch (type)
    {
    case PROGRAM_HEADER_TYPE_NULL:
        return PROGRAM_HEADER_TYPE_NULL;
    case PROGRAM_HEADER_TYPE_LOAD:
        return PROGRAM_HEADER_TYPE_LOAD;
    case PROGRAM_HEADER_TYPE_DYNAMIC:
        return PROGRAM_HEADER_TYPE_DYNAMIC;
    case PROGRAM_HEADER_TYPE_INTERP:
        return PROGRAM_HEADER_TYPE_INTERP;
    case PROGRAM_HEADER_TYPE_NOTE:
        return PROGRAM_HEADER_TYPE_NOTE;
    case PROGRAM_HEADER_TYPE_SHLIB:
        return PROGRAM_HEADER_TYPE_SHLIB;
    case PROGRAM_HEADER_TYPE_PHDR:
        return PROGRAM_HEADER_TYPE_PHDR;
    case PROGRAM_HEADER_TYPE_TLS:
        return PROGRAM_HEADER_TYPE_TLS;
    case PROGRAM_HEADER_TYPE_LOOS:
        return PROGRAM_HEADER_TYPE_LOOS;
    case PROGRAM_HEADER_TYPE_HIOS:
        return PROGRAM_HEADER_TYPE_HIOS;
    case PROGRAM_HEADER_TYPE_LOPROC:
        return PROGRAM_HEADER_TYPE_LOPROC;
    default:
        return PROGRAM_HEADER_TYPE_UNKNOWN;
    }
}

static uint64_t
program_header_parse_offset(uint8_t* prog_hdr_bytes, enum_bitsize_t bitsize, enum_endianess_t endianess)
{
	if (bitsize == ENUM_BITSIZE_32) {
		return byte_arr_to_u64(prog_hdr_bytes + PROGRAM_HEADER_FIELD_OFFSET_32, 4, endianess);
	}
	else if (bitsize == ENUM_BITSIZE_64) {
        return byte_arr_to_u64(prog_hdr_bytes + PROGRAM_HEADER_FIELD_OFFSET_64, 8, endianess);
	}
    else {
        printf("Could not parse program header offset\n");
        abort();
    }
}

static uint64_t
program_header_parse_virtual_address(uint8_t* prog_hdr_bytes, enum_bitsize_t bitsize, enum_endianess_t endianess)
{
	if (bitsize == ENUM_BITSIZE_32) {
		return byte_arr_to_u64(prog_hdr_bytes + PROGRAM_HEADER_FIELD_VADDR_32, 4, endianess);
	}
	else if (bitsize == ENUM_BITSIZE_64) {
		return byte_arr_to_u64(prog_hdr_bytes + PROGRAM_HEADER_FIELD_VADDR_64, 8, endianess);
	}
    else {
        printf("Could not parse program header virtual address\n");
        abort();
    }
}

static uint64_t
program_header_parse_physical_address(uint8_t* prog_hdr_bytes, enum_bitsize_t bitsize, enum_endianess_t endianess)
{
	if (bitsize == ENUM_BITSIZE_32) {
		return byte_arr_to_u64(prog_hdr_bytes + PROGRAM_HEADER_FIELD_PADDR_32, 4, endianess);

	}
	else if (bitsize == ENUM_BITSIZE_64) {
		return byte_arr_to_u64(prog_hdr_bytes + PROGRAM_HEADER_FIELD_PADDR_64, 8, endianess);
	}
    else {
        printf("Could not parse program header physical address\n");
        abort();
    }
}

static uint64_t
program_header_parse_file_size(uint8_t* prog_hdr_bytes, enum_bitsize_t bitsize, enum_endianess_t endianess)
{
	if (bitsize == ENUM_BITSIZE_32) {
		return byte_arr_to_u64(prog_hdr_bytes + PROGRAM_HEADER_FIELD_FILESZ_32, 4, endianess);

	}
	else if (bitsize == ENUM_BITSIZE_64) {
		return byte_arr_to_u64(prog_hdr_bytes + PROGRAM_HEADER_FIELD_FILESZ_64, 8, endianess);
	}
    else {
        printf("Could not parse program header file size\n");
        abort();
    }
}

static uint64_t
program_header_parse_memory_size(uint8_t* prog_hdr_bytes, enum_bitsize_t bitsize, enum_endianess_t endianess)
{
	if (bitsize == ENUM_BITSIZE_32) {
		return byte_arr_to_u64(prog_hdr_bytes + PROGRAM_HEADER_FIELD_MEMSZ_32, 4, endianess);
	}
	else if (bitsize == ENUM_BITSIZE_64) {
		return byte_arr_to_u64(prog_hdr_bytes + PROGRAM_HEADER_FIELD_MEMSZ_64, 8, endianess);
	}
    else {
        printf("Could not parse program header memory size\n");
        abort();
    }
}

static uint64_t
program_header_parse_align(uint8_t* prog_hdr_bytes, enum_bitsize_t bitsize, enum_endianess_t endianess)
{
	if (bitsize == ENUM_BITSIZE_32) {
        return byte_arr_to_u64(prog_hdr_bytes + PROGRAM_HEADER_FIELD_ALIGN_32, 4, endianess);
	}
	else if (bitsize == ENUM_BITSIZE_64) {
	    return byte_arr_to_u64(prog_hdr_bytes + PROGRAM_HEADER_FIELD_ALIGN_64, 8, endianess);
	}
    else {
        printf("Could not parse program header align\n");
        abort();
    }
}

static uint64_t
program_header_parse_flags(uint8_t* prog_hdr_bytes, enum_bitsize_t bitsize, enum_endianess_t endianess)
{
	if (bitsize == ENUM_BITSIZE_32) {
		return byte_arr_to_u64(prog_hdr_bytes + PROGRAM_HEADER_FIELD_FLAGS_32, 4, endianess);
	}
	else if (bitsize == ENUM_BITSIZE_64) {
		return byte_arr_to_u64(prog_hdr_bytes + PROGRAM_HEADER_FIELD_FLAGS_64, 4, endianess);
	}
    else {
        printf("Could not parse program header flags\n");
        abort();
    }
}

program_header_t*
program_header_create(uint8_t* prog_hdr_bytes, enum_bitsize_t bitsize, enum_endianess_t endianess)
{
    program_header_t* program_header = calloc(1, sizeof(program_header_t));
    program_header->type             = program_header_parse_type(prog_hdr_bytes, endianess);
    program_header->offset           = program_header_parse_offset(prog_hdr_bytes, bitsize, endianess);
    program_header->virtual_address  = program_header_parse_virtual_address(prog_hdr_bytes, bitsize, endianess);
    program_header->physical_address = program_header_parse_physical_address(prog_hdr_bytes, bitsize, endianess);
    program_header->memory_size      = program_header_parse_memory_size(prog_hdr_bytes, bitsize, endianess);
    program_header->file_size        = program_header_parse_file_size(prog_hdr_bytes, bitsize, endianess);
    program_header->align            = program_header_parse_align(prog_hdr_bytes, bitsize, endianess);
    program_header->flags            = program_header_parse_flags(prog_hdr_bytes, bitsize, endianess);
    return program_header;
}

void
program_header_destroy(program_header_t* program_header)
{
    if (program_header) {
        free(program_header);
    }
}
