#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../emu/risc_v_emu.h"

#include "endianess_converter.h"
#include "logger.h"
#include "print_utils.h"

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

// Print value of memory with corresponding permissions of an emulator.
void
print_emu_memory(risc_v_emu_t* emu, size_t start_adr, const size_t range,
                 const char size_letter)
{
    uint8_t data_size = 0;

    if (size_letter == 'b')      data_size = BYTE_SIZE;
    else if (size_letter == 'h') data_size = HALFWORD_SIZE;
    else if (size_letter == 'w') data_size = WORD_SIZE;
    else if (size_letter == 'g') data_size = GIANT_SIZE;
    else { ginger_log(ERROR, "Invalid size letter!\n"); return; }

    for (size_t i = start_adr; i < start_adr + (range * data_size); i += data_size) {
        printf("0x%lx\t", i);
        printf("Value: 0x%-20lx\t", byte_arr_to_u64(&emu->mmu->memory[i], data_size, LSB));
        printf("Perm: ");
        print_permissions(emu->mmu->permissions[i]);
        printf("\t");

        // Calculate if address is in a dirty block
        const size_t block       = i / DIRTY_BLOCK_SIZE;
        const size_t index       = block / 64; // 64 = number of bits in bitmap entry
        const size_t bit         = block % 64;
        const uint64_t shift_bit = 1;
        if ((emu->mmu->dirty_state->dirty_bitmap[index] & (shift_bit << bit)) == 0) {
            printf("Block clean\n");
        }
        else {
            printf("Block dirty\n");
        }
    }
}

void
print_emu_memory_all(risc_v_emu_t* emu)
{
    print_emu_memory(emu, 0, emu->mmu->memory_size, 'h');
}

void
print_emu_memory_allocated(risc_v_emu_t* emu)
{
    for (size_t i = 0; i < emu->mmu->curr_alloc_adr - 1; i++) {
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

        if ((emu->mmu->dirty_state->dirty_bitmap[index] & (shift_bit << bit)) == 0) {
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
    printf("\nzero\t0x%"PRIx64"\n", emu->registers[REG_ZERO]);
    printf("ra\t0x%"PRIx64"\n", emu->registers[REG_RA]);
    printf("sp\t0x%"PRIx64"\n", emu->registers[REG_SP]);
    printf("gp\t0x%"PRIx64"\n", emu->registers[REG_GP]);
    printf("tp\t0x%"PRIx64"\n", emu->registers[REG_TP]);
    printf("t0\t0x%"PRIx64"\n", emu->registers[REG_T0]);
    printf("t1\t0x%"PRIx64"\n", emu->registers[REG_T1]);
    printf("t2\t0x%"PRIx64"\n", emu->registers[REG_T2]);
    printf("fp\t0x%"PRIx64"\n", emu->registers[REG_FP]);
    printf("s1\t0x%"PRIx64"\n", emu->registers[REG_S1]);
    printf("a0\t0x%"PRIx64"\n", emu->registers[REG_A0]);
    printf("a1\t0x%"PRIx64"\n", emu->registers[REG_A1]);
    printf("a2\t0x%"PRIx64"\n", emu->registers[REG_A2]);
    printf("a3\t0x%"PRIx64"\n", emu->registers[REG_A3]);
    printf("a4\t0x%"PRIx64"\n", emu->registers[REG_A4]);
    printf("a5\t0x%"PRIx64"\n", emu->registers[REG_A5]);
    printf("a6\t0x%"PRIx64"\n", emu->registers[REG_A6]);
    printf("a7\t0x%"PRIx64"\n", emu->registers[REG_A7]);
    printf("s2\t0x%"PRIx64"\n", emu->registers[REG_S2]);
    printf("s3\t0x%"PRIx64"\n", emu->registers[REG_S3]);
    printf("s4\t0x%"PRIx64"\n", emu->registers[REG_S4]);
    printf("s5\t0x%"PRIx64"\n", emu->registers[REG_S5]);
    printf("s6\t0x%"PRIx64"\n", emu->registers[REG_S6]);
    printf("s7\t0x%"PRIx64"\n", emu->registers[REG_S7]);
    printf("s8\t0x%"PRIx64"\n", emu->registers[REG_S8]);
    printf("s9\t0x%"PRIx64"\n", emu->registers[REG_S9]);
    printf("s10\t0x%"PRIx64"\n", emu->registers[REG_S10]);
    printf("s11\t0x%"PRIx64"\n", emu->registers[REG_S11]);
    printf("t3\t0x%"PRIx64"\n", emu->registers[REG_T3]);
    printf("t4\t0x%"PRIx64"\n", emu->registers[REG_T4]);
    printf("t5\t0x%"PRIx64"\n", emu->registers[REG_T5]);
    printf("t6\t0x%"PRIx64"\n", emu->registers[REG_T6]);
    printf("pc\t0x%"PRIx64"\n", emu->registers[REG_PC]);
}

void
print_byte_array(uint8_t bytes[], size_t nb_bytes)
{
    for (size_t i = 0; i < nb_bytes; i++) {
        printf("0x%02x ", bytes[i]);
    }
    printf("\n");
}

void
print_fstat(struct stat statbuf)
{
    printf("st_dev: 0x%0lx\n", statbuf.st_dev);
    printf("st_ino: 0x%lx\n", statbuf.st_ino);
    printf("st_mode: 0x%x\n", statbuf.st_mode);
    printf("st_nlink: 0x%lx\n", statbuf.st_nlink);
    printf("st_uid: 0x%x\n", statbuf.st_uid);
    printf("st_gid: 0x%x\n", statbuf.st_gid);
    printf("st_rdev: 0x%lx\n", statbuf.st_rdev);
    printf("st_size: %ld\n", statbuf.st_size);
    printf("st_blksize: %ld\n", statbuf.st_blksize);
    printf("st_blocks: %ld\n", statbuf.st_blocks);

    // Time of last access.
    printf("statbuf.st_atim.tv_sec: %ld\n", statbuf.st_atim.tv_sec);
    printf("statbuf.st_atim.tv_nsec: %ld\n", statbuf.st_atim.tv_nsec);

    // Time of last modification.
    printf("statbuf.st_mtim.tv_sec: %ld\n", statbuf.st_mtim.tv_sec);
    printf("statbuf.st_mtim.tv_nsec: %ld\n", statbuf.st_mtim.tv_nsec);

    // Time of last status change.
    printf("statbuf.st_ctim.tv_sec: %ld\n", statbuf.st_ctim.tv_sec);
    printf("statbuf.st_ctim.tv_nsec: %ld\n", statbuf.st_ctim.tv_nsec);
}
