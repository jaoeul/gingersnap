#ifndef MMU_H
#define MMU_H

#include "../shared/vector.h"

// Amount of bytes in single block
// TODO: Tune this value for performance
#define DIRTY_BLOCK_SIZE  64

typedef struct dirty_state dirty_state_t;
typedef struct mmu         mmu_t;

typedef uint8_t            mmu_perm_t;
typedef uint8_t            mmu_alloc_error_t;
typedef uint8_t            mmu_read_error_t;
typedef uint8_t            mmu_write_error_t;

static const mmu_perm_t MMU_PERM_EXEC  = 1 << 0;
static const mmu_perm_t MMU_PERM_WRITE = 1 << 1;
static const mmu_perm_t MMU_PERM_READ  = 1 << 2;
static const mmu_perm_t MMU_PERM_RAW   = 1 << 3; // Read after write.

static const mmu_alloc_error_t MMU_ALLOC_NO_ERROR = 0;        // No error.
static const mmu_alloc_error_t MMU_ALLOC_ERROR_MEM_FULL;      // Emulator memory is already full.
static const mmu_alloc_error_t MMU_ALLOC_ERROR_WOULD_OVERRUN; // Allocation would overrun the memory size.

static const mmu_read_error_t MMU_READ_NO_ERROR = 0;           // No error.
static const mmu_read_error_t MMU_READ_ERROR_NO_PERM;          // Attempted to read from an address with no read permission.
static const mmu_read_error_t MMU_READ_ERROR_ADR_OUT_OF_RANGE; // Attempted to read from an address which is outside emulator memory.

static const mmu_write_error_t MMU_WRITE_NO_ERROR = 0;           // No error.
static const mmu_write_error_t MMU_WRITE_ERROR_NO_PERM;          // Attempted to write from an address with no read permission.
static const mmu_write_error_t MMU_WRITE_ERROR_ADR_OUT_OF_RANGE; // Attempted to write from an address which is outside emulator memory.

struct dirty_state {
    void (*make_dirty)(dirty_state_t* state, size_t address);
    void (*print)(dirty_state_t* state);
    void (*clear)(dirty_state_t* state);

    // Keeps track of blocks of memory that have been dirtied
    size_t*  dirty_blocks;
    uint64_t nb_dirty_blocks;

    // Bytes are grouped together into blocks to avoid having to do large number
    // of memsets to reset guest memory. If a byte is written to it is
    // considered dirty and the entire block which contains the dirty byte is
    // considered dirty.
    //
    // One entry in the dirty_bitmap tracks the state of 64 blocks. One block
    // per bit. 0 = clean, 1 = dirty.
    uint64_t* dirty_bitmap;
    uint64_t  nb_max_dirty_bitmaps;
};

struct mmu {
    size_t    (*allocate)(mmu_t* mmu, size_t size, uint8_t* error);
    void      (*set_permissions)(mmu_t* mmu, size_t start_adress, uint8_t permission, size_t size);
    uint8_t   (*write)(mmu_t* mmu, size_t destination_adress, const uint8_t* source_buffer, size_t size);
    uint8_t   (*read)(mmu_t* mmu, uint8_t* destination_buffer, const size_t source_adress, size_t size);
    vector_t* (*search)(mmu_t* mmu, const uint64_t needle, const char size_letter);
    void      (*print)(mmu_t* mmu, size_t start_adr, const size_t range, const char size_letter);

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
mmu_create(const size_t memory_size, const size_t base_alloc_adr);

void
mmu_destroy(mmu_t* mmu);

void
print_permissions(uint8_t perms);

#endif // MMU_H
