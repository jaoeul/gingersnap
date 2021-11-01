#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "target.h"

target_t*
target_create(const int argc, const hstring_t argv[])
{
    target_t* target = calloc(1, sizeof(target_t));
    target->argc     = argc;
    target->argv     = calloc(argc, sizeof(hstring_t));

    for (int i = 0; i < argc; i++) {
        memcpy(&target->argv[i], &argv[i], sizeof(argv[i]));
    }

    // Parse the provided program name as an elf.
    target->elf = elf_parse(target->argv[0].string);

    return target;
}

void
target_destroy(target_t* target)
{
    for (int i = 0; i < target->argc; i++) {
        if (target->argv[i].string) {
            free(target->argv[i].string);
        }
        free(target->argv);
    }
    elf_destroy(target->elf);
    free(target);
}
