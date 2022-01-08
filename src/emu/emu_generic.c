#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <syscall.h>
#include <unistd.h>
#include <pthread.h>

#include "emu_generic.h"
#include "emu_riscv.h"
#include "syscall.h"

#include "../corpus/coverage.h"
#include "../corpus/corpus.h"
#include "../shared/endianess_converter.h"
#include "../shared/logger.h"
#include "../shared/vector.h"

emu_t*
emu_generic_create(size_t memory_size, corpus_t* corpus, enum_emu_supported_archs_t arch)
{
    switch (arch)
    {
        case ENUM_EMU_SUPPORTED_ARCHS_RISC_V:
            return emu_riscv_create(memory_size, corpus);
        case ENUM_EMU_SUPPORTED_ARCHS_X86_64:
            printf("x86 support yet implemented!\n");
            abort();
            break;
    }
    return NULL;
}

void
emu_generic_destroy(emu_t* emu)
{
    switch (emu->arch)
    {
        case ENUM_EMU_SUPPORTED_ARCHS_RISC_V:
            emu_riscv_destroy(emu);
            break;
        case ENUM_EMU_SUPPORTED_ARCHS_X86_64:
            abort();
    }
}
