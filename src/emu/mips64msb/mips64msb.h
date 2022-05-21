#ifndef MIPS64_MSB
#define MIPS64_MSB

#include "../emu_stats.h"
#include "../../corpus/corpus.h"
#include "../../mmu/mmu.h"
#include "../../target/target.h"

// Max length of an CLI argument.
#define ARG_MAX 4096

typedef enum {
    MIPS64MSB_REG_R0 = 0,
    MIPS64MSB_REG_R1,
    MIPS64MSB_REG_R2,
    MIPS64MSB_REG_R3,
    MIPS64MSB_REG_R4,
    MIPS64MSB_REG_R5,
    MIPS64MSB_REG_R6,
    MIPS64MSB_REG_R7,
    MIPS64MSB_REG_R8,
    MIPS64MSB_REG_R9,
    MIPS64MSB_REG_R10,
    MIPS64MSB_REG_R11,
    MIPS64MSB_REG_R12,
    MIPS64MSB_REG_R13,
    MIPS64MSB_REG_R14,
    MIPS64MSB_REG_R15,
    MIPS64MSB_REG_R16,
    MIPS64MSB_REG_R17,
    MIPS64MSB_REG_R18,
    MIPS64MSB_REG_R19,
    MIPS64MSB_REG_R20,
    MIPS64MSB_REG_R21,
    MIPS64MSB_REG_R22,
    MIPS64MSB_REG_R23,
    MIPS64MSB_REG_R24,
    MIPS64MSB_REG_R25,
    MIPS64MSB_REG_R26,
    MIPS64MSB_REG_R27,
    MIPS64MSB_REG_R28,
    MIPS64MSB_REG_R29, // Stack pointer
    MIPS64MSB_REG_R30,
    MIPS64MSB_REG_R31,
    MIPS64MSB_REG_HI,
    MIPS64MSB_REG_LO,
    MIPS64MSB_REG_PC,
} enum_mips64msb_reg_t;

typedef struct mips64msb_s mips64msb_t;
struct mips64msb_s {
    // Should never be accessed directly other than by implementation file.
    void                    (*instructions[256])(mips64msb_t* mips, uint32_t instruction);
    uint64_t                registers[35];
    mmu_t*                  mmu;
    uint64_t                stack_size;
    enum_emu_exit_reasons_t exit_reason;
    bool                    new_coverage;
    corpus_t*               corpus; // Shared between all emulators.

    // API
    void                       (*load_elf)   (mips64msb_t* self, const target_t* target);
    void                       (*build_stack)(mips64msb_t* self, const target_t* target);
    void                       (*execute)    (mips64msb_t* self);
    mips64msb_t*               (*fork)       (const mips64msb_t* self);
    void                       (*reset)      (mips64msb_t* self, const mips64msb_t* src);
    void                       (*print_regs) (mips64msb_t* self);
    enum_emu_exit_reasons_t    (*run)        (mips64msb_t* self, emu_stats_t* stats);
    enum_emu_exit_reasons_t    (*run_until)  (mips64msb_t* self, emu_stats_t* stats, const uint64_t break_adr);
    void                       (*stack_push) (mips64msb_t* self, uint8_t bytes[], size_t nb_bytes);
    uint64_t                   (*get_pc)     (const mips64msb_t* self);
    uint64_t                   (*get_reg)    (const mips64msb_t* self, enum_mips64msb_reg_t reg);
    void                       (*set_reg)    (mips64msb_t* self, enum_mips64msb_reg_t reg, const uint64_t value);
};

mips64msb_t*
mips64msb_create(size_t memory_size, corpus_t* corpus);

void
mips64msb_destroy(mips64msb_t* mips);

#endif
