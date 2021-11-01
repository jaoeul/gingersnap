#include "config.h"

global_config_t global_config = { 0 };

void
global_config_set_verbosity(bool verbosity)
{
    global_config.verbosity = verbosity;
}

void
global_config_set_coverage(bool coverage)
{
    global_config.coverage = coverage;
}

void
global_config_set_nb_cpus(uint64_t nb_cpus)
{
    global_config.nb_cpus = nb_cpus;
}

void
global_config_set_output_dir(char* output_dir)
{
    global_config.output_dir = output_dir;
}

void
global_config_set_corpus_dir(char* corpus_dir)
{
    global_config.corpus_dir = corpus_dir;
}

void
global_config_set_target(char* target)
{
    global_config.target = target;
}

bool
global_config_get_verbosity(void)
{
    return global_config.verbosity;
}

bool
global_config_get_coverage(void)
{
    return global_config.coverage;
}

uint64_t
global_config_get_nb_cpus(void)
{
    return global_config.nb_cpus;
}

char*
global_config_get_output_dir(void)
{
    return global_config.output_dir;
}

char*
global_config_get_corpus_dir(void)
{
    return global_config.corpus_dir;
}

char*
global_config_get_target(void)
{
    return global_config.target;
}
