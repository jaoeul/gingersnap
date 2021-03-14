#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "endianess_converter.h"
#include "elf_loader.h"

void
load_elf(char* path, risc_v_emu_t* emu)
{
    // Read file in provided path as byte array
    FILE*    fileptr;
    uint8_t* elf;
    long     elf_length;

    fileptr = fopen(path, "rb");
    fseek(fileptr, 0, SEEK_END);
    elf_length = ftell(fileptr);
    rewind(fileptr);

    elf = (uint8_t*)malloc(elf_length * sizeof(uint8_t));
    if (!fread(elf, elf_length, 1, fileptr)) {
        printf("Failed to read ELF!\n");
        abort();
    }
    fclose(fileptr);

    // Verify 64 bit elf
    if (elf[4] == 1) {
        printf("Gingersnap does not support 32 bit elf files yet!\n");
        abort();
    }

    uint8_t bytes_entry_point[2];
    uint8_t bytes_program_header_offset[8];
    uint8_t bytes_nb_program_headers[2];

    uint64_t entry_point;
    uint64_t program_header_offset;
    uint64_t nb_program_headers;

    // Get bytes representing important addresses in the elf
    for (uint8_t i = 0; i < 2; i++) {
        bytes_entry_point[i] = elf[0x18 + i];
    }
    for (uint8_t i = 0; i < 8; i++) {
        bytes_program_header_offset[i] = elf[0x20 + i];
    }
    for (uint8_t i = 0; i < 2; i++) {
        bytes_nb_program_headers[i] = elf[0x38 + i];
    }

    // If we have got an least significant byte elf file
    if (elf[5] == 1) {
        entry_point           = lsb_byte_arr_to_u64(bytes_entry_point, sizeof(bytes_entry_point));
        program_header_offset = lsb_byte_arr_to_u64(bytes_program_header_offset, sizeof(bytes_program_header_offset));
        nb_program_headers    = lsb_byte_arr_to_u64(bytes_nb_program_headers, sizeof(bytes_nb_program_headers));
    }
    // Else if we have got an most significant byte elf file
    else if (elf[5] == 2) {
        // TODO
        abort();
    }
    else {
        printf("Malformed ELF header!\n");
        abort();
    }
    printf("Entry point:\t\t\t0x%lx\n", entry_point);
    printf("Program header offset:\t\t0x%lx\n", program_header_offset);
    printf("Number of program headers:\t%lu\n", nb_program_headers);

    // Parse program headers
    const size_t program_header_size = 0x38;
    const size_t program_header_base = program_header_offset;
    for (uint64_t i = 0; i < nb_program_headers; i++) {
        const size_t current_program_header = program_header_base + (program_header_size * i);
        //const size_t end_address   = start_address + program_header_size;

        // Parse the current program header
        uint8_t bytes_current_program_header[program_header_size];
        (void)  bytes_current_program_header;
        for (size_t i = 0; i < program_header_size; i++) {
            bytes_current_program_header[i] = elf[current_program_header + i];
        }

        // Parse offset
        uint8_t bytes_offset[8];
        for (size_t i = 0; i < 8; i++) {
            bytes_offset[i] = elf[(current_program_header + 0x08) + i];
        }
        // Parse virtual address
        uint8_t bytes_virtual_address[8];
        for (size_t i = 0; i < 8; i++) {
            bytes_virtual_address[i] = elf[(current_program_header + 0x10) + i];
        }
        // Parse physical address
        uint8_t bytes_physical_address[8];
        for (size_t i = 0; i < 8; i++) {
            bytes_physical_address[i] = elf[(current_program_header + 0x18) + i];
        }
        // Parse file size
        uint8_t bytes_file_size[8];
        for (size_t i = 0; i < 8; i++) {
            bytes_file_size[i] = elf[(current_program_header + 0x20) + i];
        }
        // Parse memory size
        uint8_t bytes_memory_size[8];
        for (size_t i = 0; i < 8; i++) {
            bytes_memory_size[i] = elf[(current_program_header + 0x28) + i];
        }
        // Parse align
        uint8_t bytes_align[4];
        for (size_t i = 0; i < 4; i++) {
            bytes_align[i] = elf[(current_program_header + 0x30) + i];
        }
        // Parse flags
        uint8_t bytes_flags[4];
        for (size_t i = 0; i < 4; i++) {
            bytes_flags[i] = elf[(current_program_header + 0x04) + i];
        }

        segment_t segment = {
            .offset            = lsb_byte_arr_to_u64(bytes_offset, sizeof(bytes_offset)),
            .virtual_address   = lsb_byte_arr_to_u64(bytes_virtual_address, sizeof(bytes_virtual_address)),
            .physical_address  = lsb_byte_arr_to_u64(bytes_physical_address, sizeof(bytes_physical_address)),
            .file_size         = lsb_byte_arr_to_u64(bytes_file_size, sizeof(bytes_file_size)),
            .memory_size       = lsb_byte_arr_to_u64(bytes_memory_size, sizeof(bytes_memory_size)),
            .align             = lsb_byte_arr_to_u64(bytes_align, sizeof(bytes_align)),
            .flags             = lsb_byte_arr_to_u64(bytes_flags, sizeof(bytes_flags)),
        };

        printf("***\nProgram header: %lu\n", i);
        printf("offset: %lx\n", segment.offset);
        printf("v address: %lx\n", segment.virtual_address);
        printf("p address: %lx\n", segment.physical_address);
        printf("file size: %lx\n", segment.file_size);
        printf("mem size:%lx\n", segment.memory_size);
        printf("align: %lx\n", segment.align);
        printf("flags: %lx\n", segment.flags);
        printf("***\n");
    }

    return;
}
