#ifndef EMU_GENERIC_H
#define EMU_GENERIC_H

#include "mips64msb/mips64msb.h"
#include "riscv/riscv.h"

#include "../corpus/corpus.h"
#include "../emu/emu_stats.h"
#include "../main/config.h"
#include "../mmu/mmu.h"
#include "../target/target.h"

static const uint64_t KiB = 1024;
static const uint64_t MiB = 1024 * 1024;
static const uint64_t GiB = 1024 * 1024 * 1024;

static const uint64_t EMU_TOTAL_MEM = GiB * 5;

typedef struct emu_s emu_t;
struct emu_s {
    void                       (*load_elf)         (emu_t* self, const target_t* target);
    void                       (*build_stack)      (emu_t* self, const target_t* target);
    void                       (*execute)          (emu_t* self); // Execute next instruction.
    emu_t*                     (*fork)             (const emu_t* self);
    void                       (*reset)            (emu_t* self, const emu_t* src_emu); // Resets the state of the emulator to that of another one.
    void                       (*print_regs)       (emu_t* self);
    enum_emu_exit_reasons_t    (*run)              (emu_t* self, emu_stats_t* stats); // Run an emulator until it exits or crashes. Increment exit counters.
    enum_emu_exit_reasons_t    (*run_until)        (emu_t* self, emu_stats_t* stats, const uint64_t break_adr); // Run an emulator until it exits, crashes or the program counter reaches the breakpoint.
    void                       (*stack_push)       (emu_t* self, uint8_t bytes[], size_t nb_bytes); // Pushes a specified amount of bytes onto the stack.
    enum_supported_archs_t     (*get_arch)         (const emu_t* self);
    uint64_t                   (*get_pc)           (const emu_t* self);
    uint64_t                   (*get_stack_size)   (const emu_t* self);
    mmu_t*                     (*get_mmu)          (const emu_t* self);
    enum_emu_exit_reasons_t    (*get_exit_reason)  (const emu_t* self);
    bool                       (*get_new_coverage) (const emu_t* self);
    corpus_t*                  (*get_corpus)       (const emu_t* self);

    // Should only be accessed through the member functions.
    enum_supported_archs_t arch;

    // The cpu backend. Which architecture is to be used is determined by the
    // `arch` member enum.
    union {
        riscv_t*     riscv;
        mips64msb_t* mips64msb;
    };
};

emu_t*
emu_create(enum_supported_archs_t arch, size_t memory_size, corpus_t* corpus);

void
emu_destroy(emu_t* emu);

#endif
