#ifndef BINARY_INTEGERS_H
#define BINARY_INTEGERS_H

#include <stddef.h>
#include <stdint.h>

#define BIN(bint) \
    bint_to_uint64((char*)bint, sizeof(bint))

uint64_t
bint_to_uint64(char* bint, size_t bint_len);

#endif // BINARY_INTEGERS_H
