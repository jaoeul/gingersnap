#ifndef CORPUS_H
#define CORPUS_H

#include <stdint.h>

typedef struct {
    uint8_t** inputs;
} corpus_t;

corpus_t*
corpus_create(void);

void
corpus_destroy(corpus_t* corpus);

#endif
