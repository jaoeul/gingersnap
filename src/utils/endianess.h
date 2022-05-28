#ifndef ENDIANESS_CONVERTER_H
#define ENDIANESS_CONVERTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
	ENUM_ENDIANESS_LSB = 1,
	ENUM_ENDIANESS_MSB,
} enum_endianess_t;

typedef enum {
	ENUM_BITSIZE_32 = 1,
	ENUM_BITSIZE_64,
} enum_bitsize_t;

uint64_t
byte_arr_to_u64(uint8_t* bytes, size_t nb, enum_endianess_t endianess);

void
u64_to_byte_arr(uint64_t num, uint8_t result_bytes[8], enum_endianess_t endianess);

#endif // ENDIANESS_CONVERTER_H
