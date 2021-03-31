#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include "../emu/risc_v_emu.h"

void
load_elf(char* path, risc_v_emu_t* emu);

#endif // ELF_LOADER_H
