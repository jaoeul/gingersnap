// TODO: Add functionality to remove last/first/specified index,
//       insert at specified index and sort.

#include "vector.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define VECTOR_INITIAL_CAPACITY 16

vector_t*
vector_create(size_t entry_size)
{
    vector_t* vector   = calloc(1, sizeof(vector_t));
    vector->data       = calloc(VECTOR_INITIAL_CAPACITY, entry_size);
    vector->length     = 0;
    vector->capacity   = VECTOR_INITIAL_CAPACITY;
    vector->entry_size = entry_size;

    return vector;
}

void
vector_destroy(vector_t* vector)
{
    free(vector->data);
    free(vector);
}

static void
vector_grow(vector_t* vector)
{
    vector->data     = realloc(vector->data, (vector->capacity * 2) * vector->entry_size);
    vector->capacity = vector->capacity * 2;
}

size_t
vector_append(vector_t* vector, void* data)
{
    if (vector->length >= vector->capacity) {
        vector_grow(vector);
    }
    void* dst_adr = (uint8_t*)vector->data + (vector->entry_size * vector->length);
    memcpy(dst_adr, data, vector->entry_size);
    ++vector->length;

    return vector->length - 1;
}

void*
vector_get(vector_t* vector, size_t idx)
{
    if (idx > vector->length) {
        printf("\nERROR: Out of bounds read of vector!\n");
        abort();
    }
    void* dst_adr = (uint8_t*)vector->data + (idx * vector->entry_size);
    return dst_adr;
}

size_t
vector_capacity(vector_t* vector)
{
    return vector->capacity;
}

size_t
vector_length(vector_t* vector)
{
    return vector->length;
}

size_t
vector_entry_size(vector_t* vector)
{
    return vector->entry_size;
}
