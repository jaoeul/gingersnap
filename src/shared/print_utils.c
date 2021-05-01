#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
    printf("emu->zero: 0x%lx\n", emu->registers.zero);
    printf("emu->ra: 0x%lx\n", emu->registers.ra);
    printf("emu->sp: 0x%lx\n", emu->registers.sp);
    printf("emu->gp: 0x%lx\n", emu->registers.gp);
    printf("emu->tp: 0x%lx\n", emu->registers.tp);
    printf("emu->t0: 0x%lx\n", emu->registers.t0);
    printf("emu->t1: 0x%lx\n", emu->registers.t1);
    printf("emu->t2: 0x%lx\n", emu->registers.t2);
    printf("emu->fp: 0x%lx\n", emu->registers.fp);
    printf("emu->s1: 0x%lx\n", emu->registers.s1);
    printf("emu->a0: 0x%lx\n", emu->registers.a0);
    printf("emu->a1: 0x%lx\n", emu->registers.a1);
    printf("emu->a2: 0x%lx\n", emu->registers.a2);
    printf("emu->a3: 0x%lx\n", emu->registers.a3);
    printf("emu->a4: 0x%lx\n", emu->registers.a4);
    printf("emu->a5: 0x%lx\n", emu->registers.a5);
    printf("emu->a6: 0x%lx\n", emu->registers.a6);
    printf("emu->a7: 0x%lx\n", emu->registers.a7);
    printf("emu->s2: 0x%lx\n", emu->registers.s2);
    printf("emu->s3: 0x%lx\n", emu->registers.s3);
    printf("emu->s4: 0x%lx\n", emu->registers.s4);
    printf("emu->s5: 0x%lx\n", emu->registers.s5);
    printf("emu->s6: 0x%lx\n", emu->registers.s6);
    printf("emu->s7: 0x%lx\n", emu->registers.s7);
    printf("emu->s8: 0x%lx\n", emu->registers.s8);
    printf("emu->s9: 0x%lx\n", emu->registers.s9);
    printf("emu->s10: 0x%lx\n",emu->registers.s10);
    printf("emu->s11: 0x%lx\n",emu->registers.s11);
    printf("emu->t3: 0x%lx\n", emu->registers.t3);
    printf("emu->t4: 0x%lx\n", emu->registers.t4);
    printf("emu->t5: 0x%lx\n", emu->registers.t5);
    printf("emu->t6: 0x%lx\n", emu->registers.t6);
    printf("emu->pc: 0x%lx\n", emu->registers.pc);
}

void
print_byte_array(uint8_t bytes[], size_t nb_bytes)
{
    for (size_t i = 0; i < nb_bytes; i++) {
        printf("0x%02x ", bytes[i]);
    }
    printf("\n");
}
