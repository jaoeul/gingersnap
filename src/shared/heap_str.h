#ifndef HEAP_STR
#define HEAP_STR

#include <stdint.h>

typedef struct {
    char*    str;
    uint64_t len;
} heap_str_t;

heap_str_t*
heap_str_create(const char str[]);

void
heap_str_destroy(heap_str_t* str);

#endif
