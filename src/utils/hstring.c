#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hstring.h"

hstring_t*
hstring_from(const char* src)
{
    const size_t length  = strlen(src);
    hstring_t* dst        = calloc(1, sizeof(hstring_t));
    dst->string          = calloc(length + 1, 1);
    dst->length          = length;
    memcpy(dst->string, src, dst->length);
    return dst;
}

void
hstring_set(hstring_t* dst, const char* src)
{
    if (!dst) {
        printf("String not allocated!\n");
        abort();
    }
    const size_t length = strlen(src) + 1;
    dst->string         = calloc(length, 1);
    dst->length         = length;
    memcpy(dst->string, src, length);
}

char*
hstring_get(hstring_t* string)
{
    return string->string;
}

uint64_t
hstring_lengthgth(hstring_t* string)
{
    return string->length;
}

void
hstring_destroy(hstring_t* string)
{
    free(string->string);
    free(string);
}
