#ifndef PRINT_UTILS_H
#define PRINT_UTILS_H

#include <stdint.h>

#include "../emu/risc_v_emu.h"

void
u64_binary_print(uint64_t u64);

void
print_bitmaps(uint64_t* bitmaps, uint64_t nb);

void
print_permissions(uint8_t perms);

void
print_emu_memory(risc_v_emu_t* emu, size_t start_adr, const size_t range,
                 const char size_letter);

void
print_emu_memory_allocated(risc_v_emu_t* emu);

void
print_emu_registers(risc_v_emu_t* emu);

void
print_byte_array(uint8_t bytes[], size_t nb_bytes);

#endif // PRINT_UTILS_H
