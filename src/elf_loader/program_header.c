#include <stdio.h>
#include <stdlib.h>

#include "program_header.h"

uint64_t
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

uint64_t
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

uint64_t
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

uint64_t
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

uint64_t
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

uint64_t
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

uint64_t
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
