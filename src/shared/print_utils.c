#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
