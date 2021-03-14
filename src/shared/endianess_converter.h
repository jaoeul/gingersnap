#ifndef ENDIANESS_CONVERTER_H
#define ENDIANESS_CONVERTER_H

#include <stdint.h>

uint64_t
lsb_byte_arr_to_u64(uint8_t* bytes, size_t nb);

uint64_t
msb_byte_arr_to_u64(uint8_t* bytes, size_t nb);

#endif // ENDIANESS_CONVERTER_H
