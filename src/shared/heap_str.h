#ifndef HEAP_STR_H
#define HEAP_STR_H

#include <stdint.h>

typedef struct {
    char*    str;
    uint64_t len; // Includes the terminating nullbyte.
} heap_str_t;

void
heap_str_set(heap_str_t* heap_str, const char str[]);

char*
heap_str_get(heap_str_t* heap_str);

uint64_t
heap_str_length(heap_str_t* heap_str);

void
heap_str_destroy(heap_str_t* heap_str);

#endif
