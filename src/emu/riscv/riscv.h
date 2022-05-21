#ifndef EMU_RISCV_H
#define EMU_RISCV_H

#include "../emu_stats.h"
#include "../../corpus/corpus.h"
#include "../../mmu/mmu.h"
#include "../../target/target.h"

// Max length of an CLI argument.
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

// Risc V 32i + 64i Instructions and corresponding opcode.
typedef enum {
    ENUM_RISCV_LUI                              = 0x37,
    ENUM_RISCV_AUIPC                            = 0x17,
    ENUM_RISCV_JAL                              = 0x6f,
    ENUM_RISCV_JALR                             = 0x67,
    ENUM_RISCV_BRANCH                           = 0x63,
    ENUM_RISCV_LOAD                             = 0x03,
    ENUM_RISCV_STORE                            = 0x23,
    ENUM_RISCV_ARITHMETIC_I_TYPE                = 0x13,
    ENUM_RISCV_ARITHMETIC_R_TYPE                = 0x33,
    ENUM_RISCV_FENCE                            = 0x0f,
    ENUM_RISCV_ENV                              = 0x73,
    ENUM_RISCV_ARITHMETIC_64_REGISTER_IMMEDIATE = 0x1b,
    ENUM_RISCV_ARITHMETIC_64_REGISTER_REGISTER  = 0x3b,
} enum_riscv_opcode_t;

typedef struct riscv_s riscv_t;
struct riscv_s {
    // Should never be accessed directly other than by `riscv.c`.
    void                    (*instructions[256])(struct riscv_s* emu, uint32_t instruction);
    uint64_t                registers[33];
    mmu_t*                  mmu;
    uint64_t                stack_size;
    enum_emu_exit_reasons_t exit_reason;
    bool                    new_coverage;
    corpus_t*               corpus; // Shared between all emulators.

    void                       (*load_elf)   (riscv_t* self, const target_t* target);
    void                       (*build_stack)(riscv_t* self, const target_t* target);
    void                       (*execute)    (riscv_t* self);
    riscv_t*                   (*fork)       (const riscv_t* self);
    void                       (*reset)      (riscv_t* self, const riscv_t* src);
    void                       (*print_regs) (riscv_t* self);
    enum_emu_exit_reasons_t    (*run)        (riscv_t* self, emu_stats_t* stats);
    enum_emu_exit_reasons_t    (*run_until)  (riscv_t* self, emu_stats_t* stats, const uint64_t break_adr);
    void                       (*stack_push) (riscv_t* self, uint8_t bytes[], size_t nb_bytes);
    uint64_t                   (*get_pc)     (const riscv_t* self);
    uint64_t                   (*get_reg)    (const riscv_t* self, const uint8_t reg);
    void                       (*set_reg)    (riscv_t* self, const uint8_t reg, const uint64_t value);
};

riscv_t*
riscv_create(size_t memory_size, corpus_t* corpus);

void
riscv_destroy(riscv_t* riscv);

#endif
