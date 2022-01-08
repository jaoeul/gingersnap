#ifndef RISC_V_EMU_H
#define RISC_V_EMU_H

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

emu_t*
emu_riscv_create(size_t memory_size, corpus_t* corpus);

void
emu_riscv_destroy(emu_t* emu);

#endif
