#ifndef VECTOR_H
#define VECTOR_H

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    void * data;
    size_t length;
    size_t capacity;
    size_t entry_size;
} vector_t;

vector_t*
vector_create(size_t entry_size);

void
vector_destroy(vector_t* vector);

// Copy the value pointed to by `data`to next free slot in vector.
size_t
vector_append(vector_t* vector, void* data);

void*
vector_get(vector_t* vector, size_t idx);

size_t
vector_capacity(vector_t* vector);

size_t
vector_length(vector_t* vector);

size_t
vector_entry_size(vector_t* vector);

#endif // VECTOR_H
