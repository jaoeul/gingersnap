#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "endianess_converter.h"

static uint64_t
lsb_byte_arr_to_u64(uint8_t bytes[], size_t nb)
{
    uint64_t result = 0;
    for (uint64_t i = 0; i < nb; i++) {
        // Needs cast to uint64_t to not overflow when left shifting
        result |= ((uint64_t)bytes[i]) << (8 * i);
    }
    return result;
}

static uint64_t
msb_byte_arr_to_u64(uint8_t bytes[], size_t nb)
{
    uint64_t result = 0;
    for (uint64_t i = 0; i < nb; i++) {
        // Needs cast to uint64_t to not overflow when left shifting
        result |= ((uint64_t)bytes[i]) << (8 * ((nb - i) - 1));
    }
    return result;
}

uint64_t
byte_arr_to_u64(uint8_t* bytes, size_t nb, bool endianess)
{
    return endianess ? lsb_byte_arr_to_u64(bytes, nb) : msb_byte_arr_to_u64(bytes, nb);
}
