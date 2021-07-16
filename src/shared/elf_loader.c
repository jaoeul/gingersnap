#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "endianess_converter.h"
#include "elf_loader.h"
#include "logger.h"
#include "print_utils.h"

typedef struct {
    size_t offset;
    size_t virtual_address;
    size_t physical_address;
    size_t file_size;
    size_t memory_size;
    size_t align;
    size_t flags;
} program_header_t;

// Parse an elf file and load it into the memory of an emulator.
void
load_elf(char* path, risc_v_emu_t* emu)
{
    // Read file in provided path as byte array
    FILE*    fileptr;
    uint8_t* elf;
    long     elf_length;

    printf("==============================================\n");
    printf(" Loading elf %s\n", path);
    printf("==============================================\n\n");

    fileptr = fopen(path, "rb");
    if (!fileptr) {
        ginger_log(ERROR, "Could not find specified executable: %s\n", path);
        abort();
    }

    fseek(fileptr, 0, SEEK_END);
    elf_length = ftell(fileptr);
    rewind(fileptr);

    elf = (uint8_t*)malloc(elf_length * sizeof(uint8_t));
    if (!fread(elf, elf_length, 1, fileptr)) {
        ginger_log(ERROR, "Failed to read ELF!\n");
        abort();
    }
    fclose(fileptr);

    bool     is_lsb                         = false;
    bool     is_64_bit                      = false;
    size_t   program_header_size            = 0;
    uint64_t program_header_offset          = 0;
    uint64_t nb_program_headers             = 0;
    uint8_t bytes_nb_program_headers[2]     = {0};

    // If LSB elf file
    if (elf[5] == 1) {
        is_lsb = true;
        ginger_log(INFO, "Elf is LSB\n");
    }
    // Else MSB elf file
    else if (elf[5] == 2) {
        ginger_log(INFO, "Elf is MSB\n");
        is_lsb = false;
    }
    else {
        ginger_log(INFO, "Error! Malformed ELF header!\n");
        abort();
    }

    // 32 bit elf
    if (elf[4] == 1) {
        uint8_t bytes_entry_point[4]            = {0};
        uint8_t bytes_program_header_offset[4]  = {0};

        is_64_bit            = false;
        program_header_size  = 0x20;

        for (uint8_t i = 0; i < 4; i++) {
            bytes_entry_point[i] = elf[0x18 + i];
        }
        for (uint8_t i = 0; i < 4; i++) {
            bytes_program_header_offset[i] = elf[0x1C + i];
        }
        for (uint8_t i = 0; i < 2; i++) {
            bytes_nb_program_headers[i] = elf[0x2C + i];
        }
        program_header_offset  = byte_arr_to_u64(bytes_program_header_offset, 4, is_lsb);
        emu->registers[REG_PC] = byte_arr_to_u64(bytes_entry_point, 4, is_lsb);
    }
    // 64 bit elf
    else if (elf[4] == 2) {
        uint8_t bytes_entry_point[8]            = {0};
        uint8_t bytes_program_header_offset[8]  = {0};

        is_64_bit           = true;
        program_header_size = 0x38;

        for (uint8_t i = 0; i < 8; i++) {
            bytes_entry_point[i] = elf[0x18 + i];
        }
        for (uint8_t i = 0; i < 8; i++) {
            bytes_program_header_offset[i] = elf[0x20 + i];
        }
        for (uint8_t i = 0; i < 2; i++) {
            bytes_nb_program_headers[i] = elf[0x38 + i];
        }
        program_header_offset  = byte_arr_to_u64(bytes_program_header_offset, 8, is_lsb);
        emu->registers[REG_PC] = byte_arr_to_u64(bytes_entry_point, 8, is_lsb);
    }
    else {
        ginger_log(ERROR, "Malformed ELF header!\n");
        abort();
    }
    nb_program_headers = byte_arr_to_u64(bytes_nb_program_headers, 2, is_lsb);

    ginger_log(INFO, "Setting PC to              0x%x\n", emu->registers[REG_PC]);
    ginger_log(INFO, "Program header offset:     0x%lx\n", program_header_offset);
    ginger_log(INFO, "Number of program headers: %lu\n", nb_program_headers);

    // Parse program headers
    const size_t program_header_base = program_header_offset;
    for (uint64_t i = 0; i < nb_program_headers; i++) {
        const size_t current_program_header = program_header_base + (program_header_size * i);

        // Parse the current program header
        uint8_t bytes_current_program_header[program_header_size];
        (void)  bytes_current_program_header;
        for (size_t i = 0; i < program_header_size; i++) {
            bytes_current_program_header[i] = elf[current_program_header + i];
        }

        // Parse program header type
        uint64_t program_header_type;
        uint8_t  bytes_type[4];
        for (size_t i = 0; i < 4; i++) {
            bytes_type[i] = elf[current_program_header + i];
            program_header_type = byte_arr_to_u64(bytes_type, sizeof(bytes_type), is_lsb);
        }
        // Skip this header if not loadable
        if (program_header_type != 1) {
            continue;
        }

        // Load addresses related to loadable segments into a program_header struct
        program_header_t program_header = {0};
        if (is_64_bit) {
            uint8_t bytes_offset[8];
            for (size_t i = 0; i < 8; i++) {
                bytes_offset[i] = elf[(current_program_header + 0x08) + i];
            }
            uint8_t bytes_virtual_address[8];
            for (size_t i = 0; i < 8; i++) {
                bytes_virtual_address[i] = elf[(current_program_header + 0x10) + i];
            }
            uint8_t bytes_physical_address[8];
            for (size_t i = 0; i < 8; i++) {
                bytes_physical_address[i] = elf[(current_program_header + 0x18) + i];
            }
            uint8_t bytes_file_size[8];
            for (size_t i = 0; i < 8; i++) {
                bytes_file_size[i] = elf[(current_program_header + 0x20) + i];
            }
            uint8_t bytes_memory_size[8];
            for (size_t i = 0; i < 8; i++) {
                bytes_memory_size[i] = elf[(current_program_header + 0x28) + i];
            }
            uint8_t bytes_align[8];
            for (size_t i = 0; i < 8; i++) {
                bytes_align[i] = elf[(current_program_header + 0x30) + i];
            }
            uint8_t bytes_flags[4];
            for (size_t i = 0; i < 4; i++) {
                bytes_flags[i] = elf[(current_program_header + 0x04) + i];
            }
            program_header.offset           = byte_arr_to_u64(bytes_offset, sizeof(bytes_offset), is_lsb);
            program_header.virtual_address  = byte_arr_to_u64(bytes_virtual_address, sizeof(bytes_virtual_address), is_lsb);
            program_header.physical_address = byte_arr_to_u64(bytes_physical_address, sizeof(bytes_physical_address), is_lsb);
            program_header.file_size        = byte_arr_to_u64(bytes_file_size, sizeof(bytes_file_size), is_lsb);
            program_header.memory_size      = byte_arr_to_u64(bytes_memory_size, sizeof(bytes_memory_size), is_lsb);
            program_header.align            = byte_arr_to_u64(bytes_align, sizeof(bytes_align), is_lsb);
            program_header.flags            = byte_arr_to_u64(bytes_flags, sizeof(bytes_flags), is_lsb);
        }
        else if (!is_64_bit) {
            uint8_t bytes_offset[4];
            for (size_t i = 0; i < 4; i++) {
                bytes_offset[i] = elf[(current_program_header + 0x04) + i];
            }
            uint8_t bytes_virtual_address[4];
            for (size_t i = 0; i < 4; i++) {
                bytes_virtual_address[i] = elf[(current_program_header + 0x08) + i];
            }
            uint8_t bytes_physical_address[4];
            for (size_t i = 0; i < 4; i++) {
                bytes_physical_address[i] = elf[(current_program_header + 0x0C) + i];
            }
            uint8_t bytes_file_size[4];
            for (size_t i = 0; i < 4; i++) {
                bytes_file_size[i] = elf[(current_program_header + 0x10) + i];
            }
            uint8_t bytes_memory_size[4];
            for (size_t i = 0; i < 4; i++) {
                bytes_memory_size[i] = elf[(current_program_header + 0x14) + i];
            }
            uint8_t bytes_flags[4];
            for (size_t i = 0; i < 4; i++) {
                bytes_flags[i] = elf[(current_program_header + 0x18) + i];
            }
            uint8_t bytes_align[4];
            for (size_t i = 0; i < 4; i++) {
                bytes_align[i] = elf[(current_program_header + 0x1C) + i];
            }
            program_header.offset            = byte_arr_to_u64(bytes_offset, sizeof(bytes_offset), is_lsb);
            program_header.virtual_address   = byte_arr_to_u64(bytes_virtual_address, sizeof(bytes_virtual_address), is_lsb);
            program_header.physical_address  = byte_arr_to_u64(bytes_physical_address, sizeof(bytes_physical_address), is_lsb);
            program_header.file_size         = byte_arr_to_u64(bytes_file_size, sizeof(bytes_file_size), is_lsb);
            program_header.memory_size       = byte_arr_to_u64(bytes_memory_size, sizeof(bytes_memory_size), is_lsb);
            program_header.align             = byte_arr_to_u64(bytes_align, sizeof(bytes_align), is_lsb);
            program_header.flags             = byte_arr_to_u64(bytes_flags, sizeof(bytes_flags), is_lsb);
        }

        // Sanity checks
        if ((program_header.offset + program_header.file_size) > (emu->mmu->memory_size - 1)) {
            ginger_log(ERROR, "[%s] Error! Write of 0x%lx bytes to address 0x%lx "
                    "would cause write outside of emulator memory!\n", __func__,
                    program_header.file_size, program_header.offset);
            abort();
        }

        // We have to load the elf before memory is allocated in the emulator.
        // Thus we cannot check if a write would cause the emulator go out
        // of allocated memory. No memory should be allocated at this point.
        //
        // Set the permissions of the addresses where the loadable program
        // header will be loaded to writeable. We have to do this since the
        // memory we are about to write to is not yet allocated, and does not
        // have WRITE permissions set.
        emu->mmu->set_permissions(emu->mmu, program_header.virtual_address, PERM_WRITE, program_header.file_size);

        // Load the executable segments of the binary into the emulator
        // NOTE: This write dirties the executable memory. Might want to make it
        //       clean before starting the emulator
        emu->mmu->write(emu->mmu, program_header.virtual_address, &elf[program_header.offset], program_header.file_size);

        // Set correct perms of loaded program header.
        emu->mmu->set_permissions(emu->mmu, program_header.virtual_address, program_header.flags, program_header.file_size);

        // TODO: Make permissions print out part of ginger_log().
        ginger_log(INFO, "Wrote program header %lu of size 0x%lx to virtual address 0x%lx with perms ", i,
                   program_header.file_size, program_header.virtual_address);
        print_permissions(program_header.flags);
        printf("\n");

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
