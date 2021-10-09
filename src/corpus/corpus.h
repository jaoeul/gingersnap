#ifndef CORPUS_H
#define CORPUS_H

#include <stdint.h>

#define MAX_NB_CORPUS_INPUT_FILES 1024

typedef struct {
    uint8_t* data;
    uint64_t length;
} input_t;

typedef struct {
    input_t*  inputs[MAX_NB_CORPUS_INPUT_FILES];
    uint64_t  nb_inputs;
} corpus_t;

// Create corpus structure, containing one input per file in the provided
// directory.
corpus_t*
corpus_create(const char* corpus_dir);

void
corpus_destroy(corpus_t* corpus);

#endif
