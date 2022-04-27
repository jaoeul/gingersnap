#ifndef HSTRING_H
#define HSTRING_H

#include <stdint.h>

typedef struct {
    char*    string;
    uint64_t length; // Includes the terminating nullbyte.
} hstring_t;

hstring_t*
hstring_from(const char* string);

void
hstring_set(hstring_t* string, const char str[]);

char*
hstring_get(hstring_t* string);

uint64_t
hstring_length(hstring_t* string);

void
hstring_destroy(hstring_t* string);

#endif
