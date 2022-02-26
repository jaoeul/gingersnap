#include <stdlib.h>

#include "emu_generic.h"
#include "emu_x86_64.h"

#include "../mmu/mmu.h"
#include "../shared/logger.h"

static void
emu_x86_64_execute_next_instruction(emu_t* emu)
{
}

static emu_t*
emu_x86_64_fork(const emu_t* emu)
{
    return NULL;
}

static void
emu_x86_64_reset(emu_t* dst_emu, const emu_t* src_emu)
{
}

static void
emu_x86_64_print_regs(emu_t* emu)
{
}

static enum_emu_exit_reasons_t
emu_x86_64_run (emu_t* emu, emu_stats_t* stats)
{
    return -1;
}

static enum_emu_exit_reasons_t
emu_x86_64_run_until(emu_t* emu, emu_stats_t* stats, const uint64_t break_adr)
{
    return -1;
}

static void
emu_x86_64_stack_push(emu_t* emu, uint8_t bytes[], size_t nb_bytes)
{
}

static uint64_t
emu_x86_64_get_reg(const emu_t* emu, const uint8_t reg)
{
    return 0;
}

static void
emu_x86_64_set_reg(emu_t* emu, const uint8_t reg, const uint64_t value)
{
}

static uint64_t
emu_x86_64_get_pc(const emu_t* emu)
{
    return 0;
}

static void
emu_x86_64_set_pc(emu_t* emu, const uint64_t value)
{
}

static uint64_t
emu_x86_64_get_sp(const emu_t* emu)
{
    return 0;
}

static void
emu_x86_64_set_sp
(emu_t* emu, const uint64_t value)
{
}

emu_t*
emu_x86_64_create(size_t memory_size, corpus_t* corpus)
{
    emu_t* emu = calloc(1, sizeof(emu_t));
    if (!emu) {
        ginger_log(ERROR, "[%s]Could not create emu!\n", __func__);
        return NULL;
    }

    emu->stack_size = 1024 * 1024; // 1MiB stack.

    emu->mmu = mmu_create(memory_size, emu->stack_size);
    if (!emu->mmu) {
        ginger_log(ERROR, "[%s]Could not create mmu!\n", __func__);
        abort();
    }

    emu->execute    = emu_x86_64_execute_next_instruction;
    emu->fork       = emu_x86_64_fork;
    emu->reset      = emu_x86_64_reset;
    emu->run        = emu_x86_64_run;
    emu->run_until  = emu_x86_64_run_until;
    emu->stack_push = emu_x86_64_stack_push;
    emu->print_regs = emu_x86_64_print_regs;
    emu->get_reg    = emu_x86_64_get_reg;
    emu->set_reg    = emu_x86_64_set_reg;
    emu->get_pc     = emu_x86_64_get_pc;
    emu->set_pc     = emu_x86_64_set_pc;
    emu->get_sp     = emu_x86_64_get_sp;
    emu->set_sp     = emu_x86_64_set_sp;

    return emu;
}

void
emu_x86_64_destroy(emu_t* emu)
{
    if (emu) {
        if (emu->mmu) {
            mmu_destroy(emu->mmu);
        }
        free(emu);
    }
}
