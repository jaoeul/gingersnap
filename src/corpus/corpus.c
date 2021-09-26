#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#include "corpus.h"

#include "../shared/logger.h"

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
        if (corpus->nb_inputs >= MAX_NB_CORPUS_INPUT_FILES) {
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

        corpus->inputs[corpus->nb_inputs] = calloc(input_len, sizeof(uint8_t));
        if (!fread(corpus->inputs[corpus->nb_inputs], input_len, 1, fileptr)) {
            ginger_log(ERROR, "Failed to read corpus input!\n");
            abort();
        }
        fclose(fileptr);
        ginger_log(INFO, "Added file %s containing %lu bytes to corpus.\n", base_path, input_len);

        corpus->nb_inputs++;
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

corpus_t*
corpus_create(const char* corpus_dir)
{
    corpus_t* corpus = calloc(1, sizeof(corpus_t));
    corpus_load_inputs(corpus_dir, corpus);
    return corpus;
}

void
corpus_destroy(corpus_t* corpus)
{
    for (uint64_t i = 0; i < corpus->nb_inputs; i++) {
        free(corpus->inputs[i]);
    }
    free(corpus);
}
