#ifndef CORPUS_H
#define CORPUS_H

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#include "coverage.h"

#include "../shared/vector.h"

#define MAX_NB_CORPUS_INPUTS 1024

typedef struct {
    uint8_t* data;
    uint64_t length;
} input_t;

typedef struct {
    vector_t*       inputs;
    coverage_t*     coverage;
    // Lock for synchronization of writes to the `inputs` vector.
    pthread_mutex_t lock;
} corpus_t;

// Create corpus structure, containing one input per file in the provided
// directory.
corpus_t*
corpus_create(const char* corpus_dir);

void
corpus_destroy(corpus_t* corpus);

// Copies the data from the input pointed to by `input_ptr` to the corpus.
// This needs to be thread safe, as multiple fuzzers can add inputs
// simultaneously.
bool
corpus_add_input(corpus_t* corpus, input_t* input_ptr);

void
corpus_print(corpus_t*);

input_t*
corpus_input_create(const size_t capacity);

void
corpus_input_destroy(input_t* input);

input_t*
corpus_input_copy(const input_t* src);

#endif
