#ifndef CONFIG_H
#define CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool     verbosity;
    bool     coverage;
    uint64_t nb_cpus;
    char*    output_dir;
    char*    corpus_dir;
    char*    target;
} global_config_t;

void
global_config_set_verbosity(bool verbosity);

void
global_config_set_coverage(bool coverage);

void
global_config_set_nb_cpus(uint64_t nb_cpus);

void
global_config_set_output_dir(char* output_dir);

void
global_config_set_corpus_dir(char* corpus_dir);

void
global_config_set_target(char* target);

bool
global_config_get_verbosity(void);

bool
global_config_get_coverage(void);

uint64_t
global_config_get_nb_cpus(void);

char*
global_config_get_output_dir(void);

char*
global_config_get_corpus_dir(void);

char*
global_config_get_target(void);

#endif
