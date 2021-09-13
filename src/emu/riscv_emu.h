#ifndef EMU_H
#define EMU_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../mmu/mmu.h"
#include "../shared/vector.h"
#include "../target/target.h"

enum emu_exit_reason {
    EMU_EXIT_REASON_NO_EXIT,
    EMU_EXIT_REASON_SYSCALL_NOT_SUPPORTED = 1, // Unknown syscall.
    EMU_EXIT_FSTAT_BAD_FD,                     // Not supported file descriptor.
    EMU_EXIT_GRACEFUL,                         // Program called the exit syscall.
};

// This enum is not used to store register state. It is only used as indices
// for access to correct offset in the emulator register array.
enum register_indices {
    REG_ZERO = 0,
    REG_RA,
    REG_SP,
    REG_GP,
    REG_TP,
    REG_T0,
    REG_T1,
    REG_T2,
    REG_FP,
    REG_S1,
    REG_A0,
    REG_A1,
    REG_A2,
    REG_A3,
    REG_A4,
    REG_A5,
    REG_A6,
    REG_A7,
    REG_S2,
    REG_S3,
    REG_S4,
    REG_S5,
    REG_S6,
    REG_S7,
    REG_S8,
    REG_S9,
    REG_S10,
    REG_S11,
    REG_T3,
    REG_T4,
    REG_T5,
    REG_T6,
    REG_PC,
    REG_LAST,
};

struct emu {

    // Does all the necessary setup needed before execution can begin.
    void (*setup)(rv_emu_t* emu, const target_t* target);

    // Executes the next instruction.
    void (*execute)(rv_emu_t* emu);

    // Forks the emulator.
    rv_emu_t* (*fork)(const rv_emu_t* emu);

    // Resets the state of the emulator to that of another one.
    void (*reset)(rv_emu_t* dst_emu, const rv_emu_t* src_emu);

    // Pushes a specified amount of bytes onto the stack.
    void (*stack_push)(rv_emu_t* emu, uint8_t bytes[], size_t nb_bytes);

    // Destroys the emulator.
    void (*destroy)(rv_emu_t* emu);

    // All risc v instructions are implemented as separate functions. Their opcode
    // corresponds to an index in this array of function pointers.
    void (*instructions[256])(rv_emu_t* emu, uint32_t instruction);

    uint64_t registers[33];

    uint64_t stack_size;

    // File handle table. Indexed by file descriptors.
    vector_t* files;

    // Memory management unit
    mmu_t* mmu;

    // Exit reason.
    int exit_reason;

    // Thread id of the thread which is runnig this emu.
    pid_t tid;
};

rv_emu_t* emu_create(size_t memory_size);

uint64_t get_reg(const rv_emu_t* emu, const uint8_t reg);

void set_reg(rv_emu_t* emu, const uint8_t reg, const uint64_t value);

#endif
