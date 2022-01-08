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
#include "../shared/vector.h"
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
    // Does all the necessary setup needed before execution can begin.
    void (*setup)(emu_t* emu, const target_t* target);

    // Executes the next instruction.
    void (*execute)(emu_t* emu);

    // Forks the emulator.
    emu_t* (*fork)(const emu_t* emu);

    // Resets the state of the emulator to that of another one.
    void (*reset)(emu_t* dst_emu, const emu_t* src_emu);

    // Print the register state.
    void (*print_regs)(emu_t* emu);

    // Run an emulator until it exits or crashes. Increment exit counters.
    enum_emu_exit_reasons_t (*run)(emu_t* emu, emu_stats_t* stats);

    // Run an emulator until it exits, crashes or the program counter reaches the breakpoint.
    enum_emu_exit_reasons_t (*run_until)(emu_t* emu, emu_stats_t* stats, const uint64_t break_adr);

    // Pushes a specified amount of bytes onto the stack.
    void (*stack_push)(emu_t* emu, uint8_t bytes[], size_t nb_bytes);

    // Frees all the internal data of the emulator. The `emu_t` struct itself
    // should be freed with a call to `emu_generic_destroy()`. We need to do
    // this since we want to keep the cpu structure generic, enabling different
    // architectures to allocate different internal structures.
    void (*destroy_prepare)(emu_t* emu);

    // All cpu instructions are implemented as separate functions. Their opcode
    // corresponds to an index in this array of function pointers.
    void (*instructions[256])(emu_t* emu, uint32_t instruction);

    uint64_t (*get_reg)(const emu_t* emu, const uint8_t reg);

    void (*set_reg)(emu_t* emu, const uint8_t reg, const uint64_t value);

    uint64_t (*get_pc)(const emu_t* emu);

    uint64_t registers[33];

    uint64_t stack_size;

    // Memory management unit
    mmu_t* mmu;

    // Exit reason.
    enum_emu_exit_reasons_t exit_reason;

    // If the current fuzzcase generated new coverage.
    bool new_coverage;

    // Data that the injected input is based on. Shared between all emulators.
    corpus_t* corpus;

    // Architecture of the CPU.
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
