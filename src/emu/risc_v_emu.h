#ifndef EMU_H
#define EMU_H

#include <stdint.h>
#include <stdbool.h>

#define PERM_WRITE (1 << 0)
#define PERM_READ  (1 << 1)
#define PERM_EXEC  (1 << 2)
#define PERM_RAW   (1 << 3)

typedef struct risc_v_emu risc_v_emu_t;

typedef struct mmu mmu_t;

struct mmu {
    size_t (*allocate)(mmu_t* mmu, size_t size);
    void* (*set_permissions)(mmu_t* mmu, size_t start_address, uint8_t permission, size_t size);
    void* (*write)(mmu_t* mmu, size_t destination_address, uint8_t* source_buffer, size_t size);
    void* (*read)(mmu_t* mmu, uint8_t* destination_buffer, size_t source_address, size_t size);

    // The size of the emulator memory
    size_t memory_size;

    // The memory of the emulator
    uint8_t* memory;

    // Memory permissions, each byte corresponds to the byte with the same offset in the guest memory block
    uint8_t* permissions;

    // Counter tracking number of allocated bytes in guest memory. Acts as the virtual base address of next allocation
    // for the guest.
    //
    // memory[current_allocation - 1] == last allocated address in guest memory
    size_t current_allocation;
};

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
    bool (*reset)  (risc_v_emu_t* destination_emu, struct risc_v_emu* source_emu);
    void (*destroy)(risc_v_emu_t* emu);

    // The registers, tracking the cpu emulator state
    registers_t registers;

    // Memory management unit
    mmu_t* mmu;
};

risc_v_emu_t* risc_v_emu_create(size_t memory_size);


#endif // EMU_H
