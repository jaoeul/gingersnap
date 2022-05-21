#include <string.h>

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
global_config_set_progress_dir(char* progress_dir)
{
    global_config.progress_dir = progress_dir;
}

void
global_config_set_crashes_dir(char* crashes_dir)
{
    global_config.crashes_dir = crashes_dir;
}

void
global_config_set_inputs_dir(char* inputs_dir)
{
    global_config.inputs_dir = inputs_dir;
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

void
global_config_set_arch(char* arch)
{
    if (strcmp(arch, "rv64i") == 0) {
        global_config.arch = ENUM_SUPPORTED_ARCHS_RISCV64I;
    }
    else {
        global_config.arch = ENUM_SUPPORTED_ARCHS_INVALID;
    }
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
global_config_get_progress_dir(void)
{
    return global_config.progress_dir;
}

char*
global_config_get_crashes_dir(void)
{
    return global_config.crashes_dir;
}

char*
global_config_get_inputs_dir(void)
{
    return global_config.inputs_dir;
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

enum_supported_archs_t
global_config_get_arch(void)
{
    return global_config.arch;
}
