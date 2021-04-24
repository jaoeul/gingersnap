#ifndef ENDIANESS_CONVERTER_H
#define ENDIANESS_CONVERTER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    LSB = true,
    MSB = false,
} ENDIANESS;

uint64_t
byte_arr_to_u64(uint8_t* bytes, size_t nb, ENDIANESS endianess);

void
u64_to_byte_arr(uint64_t num, uint8_t result_bytes[8], ENDIANESS endianess);

#endif // ENDIANESS_CONVERTER_H
