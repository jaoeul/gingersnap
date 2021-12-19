#ifndef PRINT_UTILS_H
#define PRINT_UTILS_H

#include <stdint.h>
#include <sys/stat.h>

enum {
    BYTE_SIZE     = 1,
    HALFWORD_SIZE = 2,
    WORD_SIZE     = 4,
    GIANT_SIZE    = 8,
} ENUM_DATA_SIZE;

void
u64_binary_print(uint64_t u64);

void
print_bitmaps(uint64_t* bitmaps, uint64_t nb);

void
print_byte_array(uint8_t bytes[], size_t nb_bytes);

void
print_fstat(struct stat statbuf);

#endif // PRINT_UTILS_H
