#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Remove?
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "endianess_converter.h"

static uint64_t
lsb_byte_arr_to_u64(uint8_t bytes[], size_t num)
{
    uint64_t result = 0;
    for (uint64_t i = 0; i < num; i++) {
        // Needs cast to uint64_t to not overflow when left shifting
        result |= ((uint64_t)bytes[i]) << (8 * i);
    }
    return result;
}

static uint64_t
msb_byte_arr_to_u64(uint8_t bytes[], size_t num)
{
    uint64_t result = 0;
    for (uint64_t i = 0; i < num; i++) {
        // Needs cast to uint64_t to not overflow when left shifting
        result |= ((uint64_t)bytes[i]) << (8 * ((num - i) - 1));
    }
    return result;
}

uint64_t
byte_arr_to_u64(uint8_t* bytes, size_t num, ENDIANESS endianess)
{
    return endianess ? lsb_byte_arr_to_u64(bytes, num) : msb_byte_arr_to_u64(bytes, num);
}

static void
u64_to_byte_arr_lsb(uint64_t num, uint8_t result_bytes[8])
{
    // For all bits in a uint64_t
    for (uint64_t i = 0; i < 64; i++) {
        // If the current bit is set
        if ((num & ((uint64_t)1 << i)) != 0) {
            // Set the corresponding bit in the result array.
            // Since the resulting value will be a uint8_t we have to run a
            // modulo operation on the shifting bit to not go out of bounds.
            // Otherwise we would write outside the result value, up to 64 bits.
            result_bytes[(uint8_t)floor(i / 8)] |= ((uint8_t)1 << (i % 8));
        }
    }
}

static void
u64_to_byte_arr_msb(uint64_t num, uint8_t result_bytes[8])
{
    // For all bits in a uint64_t
    for (uint64_t i = 0; i < 64; i++) {
        // If the current bit is set
        if ((num & ((uint64_t)1 << i)) != 0) {
            // Set the corresponding bit in the result array
            result_bytes[7 - ((uint8_t)floor(i / 8))] |= ((uint8_t)1 << (i % 8));
        }
    }
}

void
u64_to_byte_arr(uint64_t num, uint8_t result_bytes[8], ENDIANESS endianess)
{
    return endianess ? u64_to_byte_arr_lsb(num, result_bytes) : u64_to_byte_arr_msb(num, result_bytes);
}
