#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "binary_integers.h"
#include "logger.h"

uint64_t
bint_to_uint64(char* bint, size_t bint_len)
{
    if (bint_len > 65) {
        ginger_log(ERROR, "Binary integer to large!\n");
        abort();
    }

    uint64_t result = 0;
    uint64_t i      = 0;

    while (bint[i]) {
        if (bint[i] != 48 /*ASCII 0*/) {
            if (bint[i] != 49) {
                ginger_log(ERROR, "There can only be ones and zeroes in a binary integer!\n");
                abort();
            }
            result |= (uint64_t)1 << (63 - i);
        }
        ++i;
    }
    return result;
}
