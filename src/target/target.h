#ifndef TARGET_H
#define TARGET_H

#include "../shared/elf_loader.h"
#include "../shared/hstring.h"

typedef struct {
    int        argc;
    hstring_t* argv;
    elf_t*     elf;
} target_t;

target_t*
target_create(const int argc, const hstring_t argv[]);

void
target_destroy(target_t* target);

#endif
