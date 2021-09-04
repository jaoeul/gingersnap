#include <stdlib.h>
#include <string.h>

#include "heap_str.h"

heap_str_t*
heap_str_create(const char str[])
{
    const size_t len     = strlen(str);
    heap_str_t* heap_str = calloc(1, sizeof(heap_str_t));
    heap_str->str    = calloc(len + 1, 1);
    heap_str->len    = len;
    memcpy(heap_str->str, str, len);
    return heap_str;
}

void
heap_str_destroy(heap_str_t* heap_str)
{
    free(heap_str->str);
    free(heap_str);
}
