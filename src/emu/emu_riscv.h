#ifndef RISC_V_EMU_H
#define RISC_V_EMU_H

#include "../corpus/corpus.h"
#include "../emu/emu_stats.h"
#include "../mmu/mmu.h"
#include "../target/target.h"

// 256 MiB
#define EMU_TOTAL_MEM 1024 * 1024 * 256

// Max length of an cli argument of the target executable.
#define ARG_MAX 4096

typedef enum {
    RISC_V_REG_ZERO = 0,
    RISC_V_REG_RA,
    RISC_V_REG_SP,
    RISC_V_REG_GP,
    RISC_V_REG_TP,
    RISC_V_REG_T0,
    RISC_V_REG_T1,
    RISC_V_REG_T2,
    RISC_V_REG_FP,
    RISC_V_REG_S1,
    RISC_V_REG_A0,
    RISC_V_REG_A1,
    RISC_V_REG_A2,
    RISC_V_REG_A3,
    RISC_V_REG_A4,
    RISC_V_REG_A5,
    RISC_V_REG_A6,
    RISC_V_REG_A7,
    RISC_V_REG_S2,
    RISC_V_REG_S3,
    RISC_V_REG_S4,
    RISC_V_REG_S5,
    RISC_V_REG_S6,
    RISC_V_REG_S7,
    RISC_V_REG_S8,
    RISC_V_REG_S9,
    RISC_V_REG_S10,
    RISC_V_REG_S11,
    RISC_V_REG_T3,
    RISC_V_REG_T4,
    RISC_V_REG_T5,
    RISC_V_REG_T6,
    RISC_V_REG_PC,
    RISC_V_REG_LAST,
} enum_risc_v_reg_t;

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

emu_t*
emu_riscv_create(size_t memory_size, corpus_t* corpus);

void
emu_riscv_destroy(emu_t* emu);

#endif
