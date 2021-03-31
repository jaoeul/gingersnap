#ifndef EMU_H
#define EMU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../mmu/mmu.h"

typedef struct {
    uint64_t zero;
    uint64_t ra;
    uint64_t sp;
    uint64_t gp;
    uint64_t tp;
    uint64_t t0;
    uint64_t t1;
    uint64_t t2;
    uint64_t fp;
    uint64_t s1;
    uint64_t a0;
    uint64_t a1;
    uint64_t a2;
    uint64_t a3;
    uint64_t a4;
    uint64_t a5;
    uint64_t a6;
    uint64_t a7;
    uint64_t s2;
    uint64_t s3;
    uint64_t s4;
    uint64_t s5;
    uint64_t s6;
    uint64_t s7;
    uint64_t s8;
    uint64_t s9;
    uint64_t s10;
    uint64_t s11;
    uint64_t t3;
    uint64_t t4;
    uint64_t t5;
    uint64_t t6;
    uint64_t pc;
} registers_t;

struct risc_v_emu {
    void (*init)   (risc_v_emu_t* emu);
    void (*execute)(risc_v_emu_t* emu);
    bool (*fork)  (risc_v_emu_t* destination_emu, struct risc_v_emu* source_emu);
    void (*destroy)(risc_v_emu_t* emu);

    // The registers, tracking the cpu emulator state
    registers_t registers;

    // Memory management unit
    mmu_t* mmu;
};

risc_v_emu_t* risc_v_emu_create(size_t memory_size);

#endif // EMU_H
