#ifndef MMU_H
#define MMU_H

#include "../shared/vector.h"

// Amount of bytes in single block
// TODO: Tune this value for performance
#define DIRTY_BLOCK_SIZE  64;

typedef struct risc_v_emu risc_v_emu_t;

typedef struct dirty_state dirty_state_t;

typedef struct mmu mmu_t;

enum uint8_t {
    PERM_EXEC  = 1 << 0,
    PERM_WRITE = 1 << 1,
    PERM_READ  = 1 << 2,
    PERM_RAW   = 1 << 3,
};

struct dirty_state {
    void (*make_dirty)(dirty_state_t* state, size_t address);
    void (*print)(dirty_state_t* state);

    // Keeps track of blocks of memory that have been dirtied
    size_t*  dirty_blocks;
    uint64_t nb_max_dirty_blocks;
    uint64_t index_dirty_blocks;

    // Bytes are grouped together into blocks to avoid having to do large number
    // of memsets to reset guest memory. If a byte is written to it is
    // considered dirty and the entire block which contains the dirty byte is
    // considered dirty.
    //
    // One entry in the dirty_bitmap tracks the state of 64 blocks. One block
    // per bit. 0 = clean, 1 = dirty.
    uint64_t* dirty_bitmaps;
    uint64_t  nb_max_dirty_bitmaps;
};

struct mmu {
    size_t    (*allocate)(mmu_t* mmu, size_t size);
    void      (*set_permissions)(mmu_t* mmu, size_t start_adress, uint8_t permission, size_t size);
    void      (*write)(mmu_t* mmu, size_t destination_adress, const uint8_t* source_buffer, size_t size);
    void      (*read)(mmu_t* mmu, uint8_t* destination_buffer, const size_t source_adress, size_t size);
    vector_t* (*search)(mmu_t* mmu, const uint64_t needle, const char size_letter);

    // The size of the emulator memory
    size_t memory_size;

    // TODO: Check perfomance loss when using vector instead of uint8_t* as
    // memory data type. Same goes for perms.
    //
    // The memory of the emulator
    uint8_t* memory;

    // Memory permissions, each byte corresponds to the byte with the same offset in the guest memory block
    uint8_t* permissions;

    // Counter tracking number of allocated bytes in guest memory. Acts as the virtual base address of next allocation
    // for the guest.
    //
    // memory[current_allocation - 1] == last allocated address in guest memory
    size_t curr_alloc_adr;

    dirty_state_t* dirty_state;
};

mmu_t*
mmu_create(size_t memory_size);

void
mmu_destroy(mmu_t* mmu);

#endif // MMU_H
