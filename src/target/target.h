#ifndef TARGET_H
#define TARGET_H

#include "../shared/elf_loader.h"
#include "../shared/heap_str.h"

typedef struct {
    int          argc;
    heap_str_t*  argv;
    elf_t*       elf;
} target_t;

target_t*
target_create(const int argc, const heap_str_t argv[]);

void
target_destroy(target_t* target);

#endif
