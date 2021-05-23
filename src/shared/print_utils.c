#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "print_utils.h"
#include "../emu/risc_v_emu.h"

void
u64_binary_print(uint64_t u64)
{
    // Has to be 64 bits, to avoid wrapping when shifting
    uint64_t shift_bit = 1;

    for (uint64_t i = 0; i < 64; i++) {
        if ((u64 & (shift_bit << (63 - i))) == 0) {
            printf("0");
        }
        else {
            printf("1");
        }
    }
    printf("\n");
}

void
print_bitmaps(uint64_t* bitmaps, uint64_t nb)
{
    for (uint64_t i = 0; i < nb; i++) {
        printf("Bitmap %lu\t", (nb - i) - 1);
        u64_binary_print(bitmaps[(nb - i) - 1]);
    }
    printf("\n");
}

void
print_permissions(uint8_t perms)
{
    if (perms == 0) {
        printf("None");
        return;
    }
    if ((perms & (1 << 0))) {
        printf("E ");
    }
    if ((perms & (1 << 1))) {
        printf("W ");
    }
    if ((perms & (1 << 2))) {
        printf("R ");
    }
    if ((perms & (1 << 3))) {
        printf("RAW");
    }
}

// Print value of all memory with corresponding permissions of an emulator
void
print_emu_memory(risc_v_emu_t* emu)
{
    for (size_t i = 0; i < emu->mmu->memory_size - 1; i++) {
        printf("Address: 0x%lx\t", i);
        printf("Value: 0x%x\t", emu->mmu->memory[i]);
        printf("Perm: ");
        print_permissions(emu->mmu->permissions[i]);
        printf("\t");
        printf("In dirty block: ");

        // Calculate if address is in a dirty block
        const size_t block       = i / DIRTY_BLOCK_SIZE;
        const size_t index       = block / 64; // 64 = number of bits in bitmap entry
        const size_t bit         = block % 64;
        const uint64_t shift_bit = 1;
        if ((emu->mmu->dirty_state->dirty_bitmaps[index] & (shift_bit << bit)) == 0) {
            printf("NO");
        }
        else {
            printf("YES");
        }
        printf("\n");
    }
}

void
print_emu_memory_allocated(risc_v_emu_t* emu)
{
    for (size_t i = 0; i < emu->mmu->current_allocation - 1; i++) {
        printf("Address: 0x%lx\t", i);
        printf("Value: 0x%x\t", emu->mmu->memory[i]);
        printf("Perm: ");
        print_permissions(emu->mmu->permissions[i]);
        printf("\t");
        printf("In dirty block: ");

        // Calculate if address is in a dirty block
        const size_t block       = i / DIRTY_BLOCK_SIZE;
        const size_t index       = block / 64; // 64 = number of bits in bitmap entry
        const size_t bit         = block % 64;
        const uint64_t shift_bit = 1;

        if ((emu->mmu->dirty_state->dirty_bitmaps[index] & (shift_bit << bit)) == 0) {
            printf("NO");
        }
        else {
            printf("YES");
        }
        printf("\n");
    }
}

void
print_emu_registers(risc_v_emu_t* emu)
{
    // Get width of terminal window.
    unsigned short nb_columns = 80; // Default.
    struct winsize window;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &window);
    if (window.ws_col > 80) {
        nb_columns = window.ws_col;
    }

    // How many register strings fit horizontally?
    unsigned short max_len_string = 7 /*"ZERO 0x"*/ + 20 /*"0xffffffffffffffff  "*/;
    unsigned short nb_fit = nb_columns / max_len_string;

    // Strings to print.
    uint8_t nb_register_strings = 33;
    char*   register_strings[nb_register_strings];
    for (int i = 0; i < nb_register_strings; ++i) {
        register_strings[i] = calloc(max_len_string, 1);
    }
    sprintf(register_strings[0],  "ZERO 0x%-16lx", emu->registers[REG_ZERO]);
    sprintf(register_strings[1],  "RA   0x%-16lx", emu->registers[REG_RA]);
    sprintf(register_strings[2],  "SP   0x%-16lx", emu->registers[REG_SP]);
    sprintf(register_strings[3],  "GP   0x%-16lx", emu->registers[REG_GP]);
    sprintf(register_strings[4],  "TP   0x%-16lx", emu->registers[REG_TP]);
    sprintf(register_strings[5],  "T0   0x%-16lx", emu->registers[REG_T0]);
    sprintf(register_strings[6],  "T1   0x%-16lx", emu->registers[REG_T1]);
    sprintf(register_strings[7],  "T2   0x%-16lx", emu->registers[REG_T2]);
    sprintf(register_strings[8],  "FP   0x%-16lx", emu->registers[REG_FP]);
    sprintf(register_strings[9],  "S1   0x%-16lx", emu->registers[REG_S1]);
    sprintf(register_strings[10], "A0   0x%-16lx", emu->registers[REG_A0]);
    sprintf(register_strings[11], "A1   0x%-16lx", emu->registers[REG_A1]);
    sprintf(register_strings[12], "A2   0x%-16lx", emu->registers[REG_A2]);
    sprintf(register_strings[13], "A3   0x%-16lx", emu->registers[REG_A3]);
    sprintf(register_strings[14], "A4   0x%-16lx", emu->registers[REG_A4]);
    sprintf(register_strings[15], "A5   0x%-16lx", emu->registers[REG_A5]);
    sprintf(register_strings[16], "A6   0x%-16lx", emu->registers[REG_A6]);
    sprintf(register_strings[17], "A7   0x%-16lx", emu->registers[REG_A7]);
    sprintf(register_strings[18], "S2   0x%-16lx", emu->registers[REG_S2]);
    sprintf(register_strings[19], "S3   0x%-16lx", emu->registers[REG_S3]);
    sprintf(register_strings[20], "S4   0x%-16lx", emu->registers[REG_S4]);
    sprintf(register_strings[21], "S5   0x%-16lx", emu->registers[REG_S5]);
    sprintf(register_strings[22], "S6   0x%-16lx", emu->registers[REG_S6]);
    sprintf(register_strings[23], "S7   0x%-16lx", emu->registers[REG_S7]);
    sprintf(register_strings[24], "S8   0x%-16lx", emu->registers[REG_S8]);
    sprintf(register_strings[25], "S9   0x%-16lx", emu->registers[REG_S9]);
    sprintf(register_strings[26], "S10  0x%-16lx", emu->registers[REG_S1]);
    sprintf(register_strings[27], "S11  0x%-16lx", emu->registers[REG_S1]);
    sprintf(register_strings[28], "T3   0x%-16lx", emu->registers[REG_T3]);
    sprintf(register_strings[29], "T4   0x%-16lx", emu->registers[REG_T4]);
    sprintf(register_strings[30], "T5   0x%-16lx", emu->registers[REG_T5]);
    sprintf(register_strings[31], "T6   0x%-16lx", emu->registers[REG_T6]);
    sprintf(register_strings[32], "PC   0x%-16lx", emu->registers[REG_PC]);

    // Pretty print.
    char legend[nb_columns];
    memset(legend, (uint8_t)'=', nb_columns);
    printf("%s\n", legend);
    for (unsigned short i = 0; i < nb_register_strings;) {
        for (unsigned short j = 0; j < nb_fit && i < nb_register_strings; j++) {
            printf("%s", register_strings[i++]);
        }
        printf("\n");
    }
    printf("%s\n", legend);

    for (int i = 0; i < nb_register_strings; ++i) {
        free(register_strings[i]);
    }
}

void
print_byte_array(uint8_t bytes[], size_t nb_bytes)
{
    for (size_t i = 0; i < nb_bytes; i++) {
        printf("0x%02x ", bytes[i]);
    }
    printf("\n");
}
