#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
    size_t offset;           // Offset in elf file to the start of the program header.
    size_t virtual_address;  // Where in memory the program header should be loaded.
    size_t physical_address; // Where in physical memory the program header should be loaded.
    size_t file_size;        // The size of the program header on disk.
    size_t memory_size;      // The size of the program header in memory.
    size_t align;            // The alignment of the program header.
    size_t flags;            // Permissions of the programr header.
} program_header_t;

typedef struct {
    bool              is_lsb;
    bool              is_64_bit;
    uint64_t          length;      // Total number of bytes in the elf file on disc.
    uint64_t          entry_point; // Program execution entry point.
    uint64_t          nb_prg_hdrs; // Number of program headers in the elf.
    program_header_t* prg_hdrs;    // The program headers.
    uint8_t*          data;        // Elf file bytes.
} elf_t;

elf_t*
parse_elf(const char* path);

#endif // ELF_LOADER_H
