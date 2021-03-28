#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include "../emu/risc_v_emu.h"

typedef struct {
    size_t offset;
    size_t virtual_address;
    size_t physical_address;
    size_t file_size;
    size_t memory_size;
    size_t align;
    size_t flags;
} program_header_t;

void
load_elf(char* path, risc_v_emu_t* emu);

#endif // ELF_LOADER_H
