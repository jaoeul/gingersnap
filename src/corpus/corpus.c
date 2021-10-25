#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "corpus.h"
#include "coverage.h"

#include "../shared/logger.h"
#include "../shared/vector.h"

// Copy input data to the shared corpus in a thread safe manner.
bool
corpus_add_input(corpus_t* corpus, input_t* input_ptr)
{
    if (corpus->inputs->length >= MAX_NB_CORPUS_INPUTS) {
        ginger_log(ERROR, "Corpus is full!\n");
        return false;
    }

    pthread_mutex_lock(&corpus->lock);
    vector_append(corpus->inputs, input_ptr);
    pthread_mutex_unlock(&corpus->lock);
    return true;
}

// Recursively load all the files in the provided directory and subdirectories
// into the corpus.
static void
corpus_load_inputs(const char* base_path, corpus_t* corpus)
{
    struct dirent* dir_ptr        = NULL;
    DIR*           dir            = opendir(base_path);
    char           tmp_path[1024] = {0};

    // Basecase. Not a dir. Add file contents to corpus.
    if (!dir) {
        if (corpus->inputs->length >= MAX_NB_CORPUS_INPUTS) {
            ginger_log(ERROR, "Corpus is full!\n");
            abort();
        }

        FILE* fileptr = fopen(base_path, "rb");
        if (!fileptr) {
            ginger_log(ERROR, "Could not open corpus file: %s\n", base_path);
            abort();
        }

        fseek(fileptr, 0, SEEK_END);
        const uint64_t input_len = ftell(fileptr);
        rewind(fileptr);

        input_t new_input = {
            .data   = calloc(input_len, sizeof(uint8_t)),
            .length = 0,
        };

        if (!fread(new_input.data, input_len, 1, fileptr)) {
            ginger_log(ERROR, "Failed to read corpus input!\n");
            abort();
        }
        fclose(fileptr);
        new_input.length = input_len;

        if (!corpus_add_input(corpus, &new_input)) {
            ginger_log(ERROR, "Corpus to big!\n");
            abort();
        }
        ginger_log(INFO, "Added file %s containing %lu bytes to corpus.\n", base_path, input_len);
        return;
    }

    // Iterate through all files and subdirs.
    while ((dir_ptr = readdir(dir)) != NULL) {
        if (strcmp(dir_ptr->d_name, ".") != 0 && strcmp(dir_ptr->d_name, "..") != 0) {

            // Construct new directory path.
            strcpy(tmp_path, base_path);
            strcat(tmp_path, "/");
            strcat(tmp_path, dir_ptr->d_name);

            // Recursive call.
            corpus_load_inputs(tmp_path, corpus);
        }
    }
    closedir(dir);
}

void
corpus_print(corpus_t* corpus)
{
    printf("Corpus length: %lu\n", corpus->inputs->length);
    for (size_t i = 0; i < corpus->inputs->length; i++) {
        const input_t* input = vector_get(corpus->inputs, i);
        printf("input %lu data: %s\n", i, input->data);
    }
    printf("\n");
}

corpus_t*
corpus_create(const char* corpus_dir)
{
    corpus_t* corpus = calloc(1, sizeof(corpus_t));
    corpus->inputs   = vector_create(sizeof(input_t));
    corpus_load_inputs(corpus_dir, corpus);
    corpus->coverage = coverage_create();
    if (pthread_mutex_init(&corpus->lock, NULL) != 0) {
        ginger_log(ERROR, "Failed to init corpus mutex!\n");
        abort();
    }
    return corpus;
}

void
corpus_destroy(corpus_t* corpus)
{
    vector_destroy(corpus->inputs);
    free(corpus);
}

input_t*
corpus_input_create(void)
{
    input_t* input = calloc(1, sizeof(input_t));
    return input;
}

void
corpus_input_destroy(input_t* input)
{
    // `input->data` does not have to be allcoated, so we don't want to abort
    // if it is not.
    if (input->data) {
        free(input->data);
    }

    // The `input` structure itself should however always be allcated.
    if (!input) {
        ginger_log(ERROR, "[%s] Double free!\n");
        abort();
    }
    free(input);
}
