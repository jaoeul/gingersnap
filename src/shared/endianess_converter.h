#ifndef ENDIANESS_CONVERTER_H
#define ENDIANESS_CONVERTER_H

#include <stdbool.h>
#include <stdint.h>

uint64_t
byte_arr_to_u64(uint8_t* bytes, size_t nb, bool endianess);

#endif // ENDIANESS_CONVERTER_H
