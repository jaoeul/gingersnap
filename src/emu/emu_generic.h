#ifndef EMU_GENERIC_H
#define EMU_GENERIC_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "emu_stats.h"
#include "emu_generic.h"

#include "../corpus/corpus.h"
#include "../corpus/coverage.h"
#include "../mmu/mmu.h"
#include "../utils/vector.h"
#include "../target/target.h"

// 256 MiB
#define EMU_TOTAL_MEM 1024 * 1024 * 256

// Max length of an cli argument of the target executable.
#define ARG_MAX 4096

typedef enum {
    ENUM_EMU_SUPPORTED_ARCHS_RISC_V,
    ENUM_EMU_SUPPORTED_ARCHS_X86_64,
} enum_emu_supported_archs_t;

typedef struct emu_s emu_t;
struct emu_s {
    // Arch independent functions.
    void (*load_elf)   (emu_t* emu, const target_t* target);
    void (*build_stack)(emu_t* emu, const target_t* target);

    // Arch dependent functions.
    void                    (*setup)            (emu_t* emu, const target_t* target); // Does all the necessary setup needed before execution can begin.
    void                    (*execute)          (emu_t* emu);      // Executes the next instruction.
    emu_t*                  (*fork)             (const emu_t* emu); // Forks the emulator.
    void                    (*reset)            (emu_t* dst_emu, const emu_t* src_emu); // Resets the state of the emulator to that of another one.
    void                    (*print_regs)       (emu_t* emu);
    enum_emu_exit_reasons_t (*run)              (emu_t* emu, emu_stats_t* stats); // Run an emulator until it exits or crashes. Increment exit counters.
    enum_emu_exit_reasons_t (*run_until)        (emu_t* emu, emu_stats_t* stats, const uint64_t break_adr); // Run an emulator until it exits, crashes or the program counter reaches the breakpoint.
    void                    (*stack_push)       (emu_t* emu, uint8_t bytes[], size_t nb_bytes); // Pushes a specified amount of bytes onto the stack.
    uint64_t                (*get_reg)          (const emu_t* emu, const uint8_t reg);
    uint64_t                (*get_pc)           (const emu_t* emu);
    uint64_t                (*get_sp)           (const emu_t* emu);
    void                    (*set_reg)          (emu_t* emu, const uint8_t reg, const uint64_t value);
    void                    (*set_pc)           (emu_t* emu, const uint64_t value);
    void                    (*set_sp)           (emu_t* emu, const uint64_t value);
    void                    (*instructions[256])(emu_t* emu, uint32_t instruction);

    uint64_t                   registers[33];
    uint64_t                   stack_size;
    mmu_t*                     mmu;
    enum_emu_exit_reasons_t    exit_reason;
    bool                       new_coverage;
    corpus_t*                  corpus; // Shared between all emulators.
    enum_emu_supported_archs_t arch;
};

// Create an emulator with specified memory size, corpus and architecture.
// The corresponding destroy function pointer is implementation-specific and
// set as a member in the resulting object. The implementation specefic
// `create` functions need to set all the functions pointers, in order to avoid
// segfaults when calling them. If the function is not needed, it should be
// implemented as a empty function.
emu_t*
emu_generic_create(size_t memory_size, corpus_t* corpus, enum_emu_supported_archs_t arch);

void
emu_generic_destroy(emu_t* emu);

#endif
