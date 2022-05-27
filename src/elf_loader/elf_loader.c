#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elf_loader.h"
#include "program_header.h"

#include "../utils/endianess.h"

static enum_elf_type_t
parse_type(uint8_t* elf_bytes)
{
    const uint8_t type = elf_bytes[ELF_HEADER_FIELD_TYPE];
    bool valid_type = false;

    for (int i = ELF_TYPE_NONE; i < ELF_TYPE_HIPROC; i++) {
        if (type == i) {
            valid_type = true;
        }
    }
    if (!valid_type) {
        printf("Invalid elf type!\n");
        abort();
    }
    return type;
}

static enum_bitsize_t
parse_bitsize(uint8_t* elf_bytes)
{
    const uint8_t bitsize = elf_bytes[ELF_HEADER_FIELD_EICLASS];

	if (bitsize != ENUM_BITSIZE_32 && bitsize != ENUM_BITSIZE_64) {
		printf("Invalid elf bitsize\n");
		abort();
	}
    else {
        return bitsize;
    }
}

static enum_endianess_t
parse_endianess(uint8_t* elf_bytes)
{
    const uint8_t endianess = elf_bytes[ELF_HEADER_FIELD_EIDATA];

	if (endianess != ENUM_ENDIANESS_LSB && endianess != ENUM_ENDIANESS_MSB) {
		printf("Invalid elf endianess\n");
		abort();
	}
    else {
        return endianess;
    }
}

static uint64_t
parse_entry_point(uint8_t* elf_bytes, enum_bitsize_t bitsize, enum_endianess_t endianess)
{
	if (bitsize == ENUM_BITSIZE_32) {
		return byte_arr_to_u64(elf_bytes + ELF_HEADER_FIELD_ENTRY, 4, endianess);
	}
	else if (bitsize == ENUM_BITSIZE_64) {
		return byte_arr_to_u64(elf_bytes + ELF_HEADER_FIELD_ENTRY, 8, endianess);
	}
    else {
        printf("Could not parse elf entry point!\n");
        abort();
    }
}

static uint64_t
parse_program_header_offset(uint8_t* elf_bytes, enum_bitsize_t bitsize, enum_endianess_t endianess)
{
	if (bitsize == ENUM_BITSIZE_32) {
		return byte_arr_to_u64(elf_bytes + ELF_HEADER_FIELD_PHOFF_32, 4, endianess);
	}
	else if (bitsize == ENUM_BITSIZE_64) {
		return byte_arr_to_u64(elf_bytes + ELF_HEADER_FIELD_PHOFF_64, 8, endianess);
	}
    else {
        printf("Could not parse elf program header offset!\n");
        abort();
    }
}

static uint64_t
parse_num_program_headers(uint8_t* elf_bytes, enum_bitsize_t bitsize, enum_endianess_t endianess)
{
	if (bitsize == ENUM_BITSIZE_32) {
		return byte_arr_to_u64(elf_bytes + ELF_HEADER_FIELD_PHNUM_32, 2, endianess);
	}
	else if (bitsize == ENUM_BITSIZE_64) {
		return byte_arr_to_u64(elf_bytes + ELF_HEADER_FIELD_PHNUM_64, 2, endianess);
	}
    else {
        printf("Could not parse the number of program headers in elf!n");
        abort();
    }
}

static uint64_t
parse_program_header_size(uint8_t* elf_bytes, enum_bitsize_t bitsize, enum_endianess_t endianess)
{
	if (bitsize == ENUM_BITSIZE_32) {
		return byte_arr_to_u64(elf_bytes + ELF_HEADER_FIELD_PHENTSIZE_32, 2, endianess);
	}
	else if (bitsize == ENUM_BITSIZE_64) {
		return byte_arr_to_u64(elf_bytes + ELF_HEADER_FIELD_PHENTSIZE_64, 2, endianess);
	}
    else {
        printf("Could not parse elf program header size!\n");
        abort();
    }
}

void
parse_program_headers(elf_t* elf)
{
    uint64_t program_header_base = elf->program_header_offset;
    for (uint64_t i = 0; i < elf->nb_program_headers; i++) {
        uint64_t curr_prog_hdr_offset = program_header_base + (elf->program_header_size * i);

        program_header_t program_header = {
            .offset           = program_header_parse_offset(&elf->data[curr_prog_hdr_offset], elf->bitsize, elf->endianess),
            .virtual_address  = program_header_parse_virtual_address(&elf->data[curr_prog_hdr_offset], elf->bitsize, elf->endianess),
            .physical_address = program_header_parse_physical_address(&elf->data[curr_prog_hdr_offset], elf->bitsize, elf->endianess),
            .memory_size      = program_header_parse_memory_size(&elf->data[curr_prog_hdr_offset], elf->bitsize, elf->endianess),
            .file_size        = program_header_parse_file_size(&elf->data[curr_prog_hdr_offset], elf->bitsize, elf->endianess),
            .align            = program_header_parse_align(&elf->data[curr_prog_hdr_offset], elf->bitsize, elf->endianess),
            .flags            = program_header_parse_flags(&elf->data[curr_prog_hdr_offset], elf->bitsize, elf->endianess),
        };
        elf->program_headers[i] = program_header;
    }
}

static char*
elf_type_to_str(enum_elf_type_t type)
{
    switch (type)
    {
    case ELF_TYPE_NONE:
        return "None";
    case ELF_TYPE_REL:
        return "Relocatable file";
    case ELF_TYPE_EXEC:
        return "Executable file";
    case ELF_TYPE_DYN:
        return "Shared object";
    case ELF_TYPE_CORE:
        return "Core file";
    case ELF_TYPE_LOOS:
        return "OS specific";
    case ELF_TYPE_HIOS:
        return "OS specific";
    case ELF_TYPE_LOPROC:
        return "Processor specific";
    case ELF_TYPE_HIPROC:
        return "Processor specific";
    }
    return "Unknown";
}

void
elf_print_append(char* buf, size_t row_length, char* prefix, char* value)
{
    char formatter[128] = {0};
    char entry[128]     = {0};

    sprintf(formatter, "%s%%-%lds|\n", prefix, row_length - strlen(prefix));

    // Evaluate the formatter, formating the value as the given type.
    sprintf(entry, formatter, value);
    strcat(buf, entry);
}

void
elf_print(const elf_t* elf)
{
    char buf[1024]  = {0};
    size_t row_size = 46;

    strcat(buf, "+==================== ELF ====================+\n");
    elf_print_append(buf, row_size, "| Path: ", elf->path);
    elf_print_append(buf, row_size, "| Type: ", elf_type_to_str(elf->type));
    elf_print_append(buf, row_size, "| Bitsize: ", elf->bitsize == ENUM_BITSIZE_32 ? "32" : "64");
    elf_print_append(buf, row_size, "| Endianess: ", elf->endianess == ENUM_ENDIANESS_LSB ? "LSB" : "MSB");

    char entry_str[64] = {0};
    sprintf(entry_str, "%lx", elf->entry_point);
    elf_print_append(buf, row_size, "| Entry point: 0x", entry_str);

    char nb_program_headers_str[64] = {0};
    sprintf(nb_program_headers_str, "%lu", elf->nb_program_headers);
    elf_print_append(buf, row_size, "| Program headers: ", nb_program_headers_str);

    char program_header_offset_str[64] = {0};
    sprintf(program_header_offset_str, "%lu", elf->program_header_offset);
    elf_print_append(buf, row_size, "| Program headers: ", program_header_offset_str);

    char program_header_size_str[64] = {0};
    sprintf(program_header_size_str, "%lu", elf->program_header_size);
    elf_print_append(buf, row_size, "| Program header size: ", program_header_size_str);

    strcat(buf, "+---------------------------------------------+\n");

    printf("%s", buf);
}

elf_t*
elf_create(char* path)
{
	elf_t* elf = calloc(1, sizeof(elf_t));

	FILE* file_pointer = fopen(path, "rb");
	if (!file_pointer){
		printf("Could not open file/file NULL\n");
		abort();
	}

	fseek(file_pointer, 0, SEEK_END);
	elf->data_length = ftell(file_pointer);
	rewind(file_pointer);

	elf->data = calloc(elf->data_length, sizeof(uint8_t));
	if(!fread(elf->data, elf->data_length, 1, file_pointer)) {
		printf("Failed to read elf\n");
		abort();
	}
	fclose(file_pointer);

    elf->path                  = path;
    elf->type                  = parse_type(elf->data);
	elf->bitsize               = parse_bitsize(elf->data);
	elf->endianess             = parse_endianess(elf->data);
	elf->entry_point           = parse_entry_point(elf->data, elf->bitsize, elf->endianess);
	elf->nb_program_headers    = parse_num_program_headers(elf->data, elf->bitsize, elf->endianess);
	elf->program_header_offset = parse_program_header_offset(elf->data, elf->bitsize, elf->endianess);
	elf->program_header_size   = parse_program_header_size(elf->data, elf->bitsize, elf->endianess);

    elf->program_headers = calloc(elf->nb_program_headers, sizeof(program_header_t));
	parse_program_headers(elf);

    elf_print(elf);

	return elf;
}

void
elf_destroy(elf_t* elf)
{
    if (elf) {
        if (elf->program_headers) {
            free(elf->program_headers);
        }
        free(elf);
    }
}
