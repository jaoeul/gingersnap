#include <stdlib.h>
#include <string.h>

#include "heap_str.h"

void
heap_str_set(heap_str_t* heap_str, const char* str)
{
    const size_t len     = strlen(str) + 1;
    heap_str->str        = calloc(len, 1);
    heap_str->len        = len;
    memcpy(heap_str->str, str, len);
}

char*
heap_str_get(heap_str_t* heap_str)
{
    return heap_str->str;
}

uint64_t
heap_str_length(heap_str_t* heap_str)
{
    return heap_str->len;
}

void
heap_str_destroy(heap_str_t* heap_str)
{
    free(heap_str->str);
}
