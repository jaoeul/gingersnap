#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "endianess_converter.h"
#include "elf_loader.h"
#include "logger.h"
#include "print_utils.h"

elf_t*
parse_elf(const char* path)
{
    elf_t* elf = calloc(1, sizeof(elf_t));

    ginger_log(INFO, "Loading elf %s\n", path);

    FILE* fileptr = fopen(path, "rb");
    if (!fileptr) {
        ginger_log(ERROR, "Could not find specified executable: %s\n", path);
        abort();
    }

    fseek(fileptr, 0, SEEK_END);
    elf->length = ftell(fileptr);
    rewind(fileptr);

    elf->data = calloc(elf->length, sizeof(uint8_t));
    if (!fread(elf->data, elf->length, 1, fileptr)) {
        ginger_log(ERROR, "Failed to read ELF!\n");
        abort();
    }
    fclose(fileptr);

    size_t   program_header_size        = 0;
    uint64_t program_header_offset      = 0;
    uint8_t bytes_nb_program_headers[2] = {0};

    // If LSB elf file
    if (elf->data[5] == 1) {
        elf->is_lsb = true;
        ginger_log(INFO, "Elf is LSB\n");
    }
    // Else MSB elf file
    else if (elf->data[5] == 2) {
        ginger_log(INFO, "Elf is MSB\n");
        elf->is_lsb = false;
    }
    else {
        ginger_log(INFO, "Error! Malformed ELF header!\n");
        abort();
    }

    // 32 bit elf
    if (elf->data[4] == 1) {
        uint8_t bytes_entry_point[4]            = {0};
        uint8_t bytes_program_header_offset[4]  = {0};

        elf->is_64_bit           = false;
        program_header_size = 0x20;

        for (uint8_t i = 0; i < 4; i++) {
            bytes_entry_point[i] = elf->data[0x18 + i];
        }
        for (uint8_t i = 0; i < 4; i++) {
            bytes_program_header_offset[i] = elf->data[0x1C + i];
        }
        for (uint8_t i = 0; i < 2; i++) {
            bytes_nb_program_headers[i] = elf->data[0x2C + i];
        }
        program_header_offset  = byte_arr_to_u64(bytes_program_header_offset, 4, elf->is_lsb);
        elf->entry_point       = byte_arr_to_u64(bytes_entry_point, 4, elf->is_lsb);
    }
    // 64 bit elf
    else if (elf->data[4] == 2) {
        uint8_t bytes_entry_point[8]           = {0};
        uint8_t bytes_program_header_offset[8] = {0};

        elf->is_64_bit           = true;
        program_header_size = 0x38;

        for (uint8_t i = 0; i < 8; i++) {
            bytes_entry_point[i] = elf->data[0x18 + i];
        }
        for (uint8_t i = 0; i < 8; i++) {
            bytes_program_header_offset[i] = elf->data[0x20 + i];
        }
        for (uint8_t i = 0; i < 2; i++) {
            bytes_nb_program_headers[i] = elf->data[0x38 + i];
        }
        program_header_offset  = byte_arr_to_u64(bytes_program_header_offset, 8, elf->is_lsb);
        elf->entry_point       = byte_arr_to_u64(bytes_entry_point, 8, elf->is_lsb);
    }
    else {
        ginger_log(ERROR, "Malformed ELF header!\n");
        abort();
    }
    elf->nb_program_headers = byte_arr_to_u64(bytes_nb_program_headers, 2, elf->is_lsb);
    elf->prg_hdrs           = calloc(elf->nb_program_headers, sizeof(program_header_t));

    ginger_log(INFO, "Program header offset:     0x%lx\n", program_header_offset);
    ginger_log(INFO, "Number of program headers: %lu\n", elf->nb_program_headers);

    // Parse program headers
    const size_t program_header_base = program_header_offset;
    for (uint64_t i = 0; i < elf->nb_program_headers; i++) {
        const size_t current_program_header = program_header_base + (program_header_size * i);

        // Parse the current program header
        uint8_t bytes_current_program_header[program_header_size];
        (void)  bytes_current_program_header;
        for (size_t i = 0; i < program_header_size; i++) {
            bytes_current_program_header[i] = elf->data[current_program_header + i];
        }

        // Parse program header type
        uint64_t program_header_type;
        uint8_t  bytes_type[4];
        for (size_t i = 0; i < 4; i++) {
            bytes_type[i] = elf->data[current_program_header + i];
            program_header_type = byte_arr_to_u64(bytes_type, sizeof(bytes_type), elf->is_lsb);
        }
        // Skip this header if not loadable
        if (program_header_type != 1) {
            continue;
        }

        // Load addresses related to loadable segments into a program_header struct
        program_header_t program_header = {0};
        if (elf->is_64_bit) {
            uint8_t bytes_offset[8];
            for (size_t i = 0; i < 8; i++) {
                bytes_offset[i] = elf->data[(current_program_header + 0x08) + i];
            }
            uint8_t bytes_virtual_address[8];
            for (size_t i = 0; i < 8; i++) {
                bytes_virtual_address[i] = elf->data[(current_program_header + 0x10) + i];
            }
            uint8_t bytes_physical_address[8];
            for (size_t i = 0; i < 8; i++) {
                bytes_physical_address[i] = elf->data[(current_program_header + 0x18) + i];
            }
            uint8_t bytes_file_size[8];
            for (size_t i = 0; i < 8; i++) {
                bytes_file_size[i] = elf->data[(current_program_header + 0x20) + i];
            }
            uint8_t bytes_memory_size[8];
            for (size_t i = 0; i < 8; i++) {
                bytes_memory_size[i] = elf->data[(current_program_header + 0x28) + i];
            }
            uint8_t bytes_align[8];
            for (size_t i = 0; i < 8; i++) {
                bytes_align[i] = elf->data[(current_program_header + 0x30) + i];
            }
            uint8_t bytes_flags[4];
            for (size_t i = 0; i < 4; i++) {
                bytes_flags[i] = elf->data[(current_program_header + 0x04) + i];
            }
            program_header.offset           = byte_arr_to_u64(bytes_offset, sizeof(bytes_offset), elf->is_lsb);
            program_header.virtual_address  = byte_arr_to_u64(bytes_virtual_address, sizeof(bytes_virtual_address), elf->is_lsb);
            program_header.physical_address = byte_arr_to_u64(bytes_physical_address, sizeof(bytes_physical_address), elf->is_lsb);
            program_header.file_size        = byte_arr_to_u64(bytes_file_size, sizeof(bytes_file_size), elf->is_lsb);
            program_header.memory_size      = byte_arr_to_u64(bytes_memory_size, sizeof(bytes_memory_size), elf->is_lsb);
            program_header.align            = byte_arr_to_u64(bytes_align, sizeof(bytes_align), elf->is_lsb);
            program_header.flags            = byte_arr_to_u64(bytes_flags, sizeof(bytes_flags), elf->is_lsb);
        }
        else if (!elf->is_64_bit) {
            uint8_t bytes_offset[4];
            for (size_t i = 0; i < 4; i++) {
                bytes_offset[i] = elf->data[(current_program_header + 0x04) + i];
            }
            uint8_t bytes_virtual_address[4];
            for (size_t i = 0; i < 4; i++) {
                bytes_virtual_address[i] = elf->data[(current_program_header + 0x08) + i];
            }
            uint8_t bytes_physical_address[4];
            for (size_t i = 0; i < 4; i++) {
                bytes_physical_address[i] = elf->data[(current_program_header + 0x0C) + i];
            }
            uint8_t bytes_file_size[4];
            for (size_t i = 0; i < 4; i++) {
                bytes_file_size[i] = elf->data[(current_program_header + 0x10) + i];
            }
            uint8_t bytes_memory_size[4];
            for (size_t i = 0; i < 4; i++) {
                bytes_memory_size[i] = elf->data[(current_program_header + 0x14) + i];
            }
            uint8_t bytes_flags[4];
            for (size_t i = 0; i < 4; i++) {
                bytes_flags[i] = elf->data[(current_program_header + 0x18) + i];
            }
            uint8_t bytes_align[4];
            for (size_t i = 0; i < 4; i++) {
                bytes_align[i] = elf->data[(current_program_header + 0x1C) + i];
            }
            program_header.offset           = byte_arr_to_u64(bytes_offset, sizeof(bytes_offset), elf->is_lsb);
            program_header.virtual_address  = byte_arr_to_u64(bytes_virtual_address, sizeof(bytes_virtual_address), elf->is_lsb);
            program_header.physical_address = byte_arr_to_u64(bytes_physical_address, sizeof(bytes_physical_address), elf->is_lsb);
            program_header.file_size        = byte_arr_to_u64(bytes_file_size, sizeof(bytes_file_size), elf->is_lsb);
            program_header.memory_size      = byte_arr_to_u64(bytes_memory_size, sizeof(bytes_memory_size), elf->is_lsb);
            program_header.align            = byte_arr_to_u64(bytes_align, sizeof(bytes_align), elf->is_lsb);
            program_header.flags            = byte_arr_to_u64(bytes_flags, sizeof(bytes_flags), elf->is_lsb);
        }
        elf->prg_hdrs[i] = program_header;
    }
    return elf;
}
