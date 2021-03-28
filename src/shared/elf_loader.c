#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "endianess_converter.h"
#include "elf_loader.h"
#include "print_utils.h"

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
        // Set the program counter to the entry point of executable
        const uint64_t entry_point = lsb_byte_arr_to_u64(bytes_entry_point, sizeof(bytes_entry_point));
        emu->registers.pc = entry_point;
        printf("[%s] Setting PC to 0x%lx\n", __func__, entry_point);

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

        // Parse program header type - Skip this header if not loadable
        uint64_t program_header_type;
        uint8_t bytes_type[4];
        for (size_t i = 0; i < 4; i++) {
            bytes_type[i] = elf[current_program_header + i];
            program_header_type = lsb_byte_arr_to_u64(bytes_type, sizeof(bytes_type));
        }
        if (program_header_type != 1) {
            continue;
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

        // Creating this struct is a bit unecessary, but might be useful for
        // code resue
        program_header_t program_header = {
            .offset            = lsb_byte_arr_to_u64(bytes_offset, sizeof(bytes_offset)),
            .virtual_address   = lsb_byte_arr_to_u64(bytes_virtual_address, sizeof(bytes_virtual_address)),
            .physical_address  = lsb_byte_arr_to_u64(bytes_physical_address, sizeof(bytes_physical_address)),
            .file_size         = lsb_byte_arr_to_u64(bytes_file_size, sizeof(bytes_file_size)),
            .memory_size       = lsb_byte_arr_to_u64(bytes_memory_size, sizeof(bytes_memory_size)),
            .align             = lsb_byte_arr_to_u64(bytes_align, sizeof(bytes_align)),
            .flags             = lsb_byte_arr_to_u64(bytes_flags, sizeof(bytes_flags)),
        };

        // Sanity checks
        if ((program_header.offset + program_header.file_size) > (emu->mmu->memory_size - 1)) {
            fprintf(stderr, "[%s] Error! Write of 0x%lx bytes to address 0x%lx "
                    "would cause write outside of emulator memory!\n", __func__,
                    program_header.file_size, program_header.offset);
            abort();
        }
        else if ((program_header.offset + program_header.file_size) > (emu->mmu->current_allocation)) {
            fprintf(stderr, "[%s] Error! Write of 0x%lx bytes to address 0x%lx "
                    "would cause write outside of emulator allocated memory!\n", __func__,
                    program_header.file_size, program_header.offset);
            abort();
        }
        // Load the executable segments of the binary into the emulator
        // NOTE: This write dirties the executable memory. Might want to make it
        //       clean before starting the emulator, for perf
        emu->mmu->write(emu->mmu, program_header.offset, elf, program_header.file_size);

        // Set the permissions of the loaded segment in the emulator
        emu->mmu->set_permissions(emu->mmu, program_header.offset, program_header.flags, program_header.file_size);

        //printf("***\nProgram header: %lu\n", i);
        //printf("offset: %lx\n", segment.offset);
        //printf("v address: %lx\n", segment.virtual_address);
        //printf("p address: %lx\n", segment.physical_address);
        //printf("file size: %lx\n", segment.file_size);
        //printf("mem size:%lx\n", segment.memory_size);
        //printf("align: %lx\n", segment.align);
        //printf("flags: %lx\n", segment.flags);
        //print_permissions(segment.flags);
        //printf("***\n");
    }

    free(elf);
    return;
}
