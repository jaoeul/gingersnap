#include <stdlib.h>

#include "corpus.h"

corpus_t*
corpus_create(void)
{
    corpus_t* corpus = calloc(1, sizeof(corpus));
    // TODO: Read and allocate inputs from file.
    return corpus;
}

void
corpus_destroy(corpus_t* corpus)
{
    free(corpus);
}
