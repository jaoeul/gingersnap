#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "endianess.h"
#include "logger.h"
#include "print_utils.h"

void
u8_binary_print(uint8_t u8)
{
    uint32_t shift_bit = 1;

    for (uint8_t i = 0; i < 8; i++) {
        if ((u8 & (shift_bit << (7 - i))) == 0) {
            printf("0");
        }
        else {
            printf("1");
        }
    }
}

void
u16_binary_print(uint16_t u16)
{
    uint16_t shift_bit = 1;

    for (uint16_t i = 0; i < 16; i++) {
        if ((u16 & (shift_bit << (15 - i))) == 0) {
            printf("0");
        }
        else {
            printf("1");
        }
    }
}

void
u32_binary_print(uint32_t u32)
{
    uint32_t shift_bit = 1;

    for (uint32_t i = 0; i < 32; i++) {
        if ((u32 & (shift_bit << (31 - i))) == 0) {
            printf("0");
        }
        else {
            printf("1");
        }
    }
}

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
