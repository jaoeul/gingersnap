#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mmu.h"

#include "../shared/print_utils.h"

static void
dirty_state_print_blocks(dirty_state_t* state)
{
    for (size_t i = 0; i < state->index_dirty_blocks; i++) {
        printf("%lu ", state->dirty_blocks[i]);
    }
    printf("\n");
}

// Mark to block which contains the address dirty by adding the block's index
// to the dirty_blocks vector, and mark the bit corresponding to the block as
// dirty in the dirty_bitmap.
static void
dirty_state_make_dirty(dirty_state_t* state, size_t block)
{
    // There are 64 blocks in one bitmap entry since we use uint64_t to
    // represent bitmap entries.
    size_t index = block / 64;
    uint8_t bit  = block % 64;

    const uint64_t shift_bit = 1;

    // If block is not already dirty
    if ((state->dirty_bitmaps[index] & (shift_bit << bit)) == 0) {
        // Append the block index of the dirty block
        state->dirty_blocks[state->index_dirty_blocks] = block;
        state->index_dirty_blocks++;

        // Mark block as dirty
        state->dirty_bitmaps[index] |= 1 << bit;
    }

    return;
}

static dirty_state_t*
dirty_state_create(size_t memory_size)
{
    dirty_state_t* state    = calloc(1, sizeof(*state));

    // Max possible number of dirty memory blocks. This is capped to the total
    // memory size / DIRTY_BLOCK_SIZE since we will not allow duplicates of
    // dirtied blocks in the dirty_state->dirty_blocks vector, and will
    // therefore never need more than this number of entries.
    size_t nb_max_blocks = memory_size / DIRTY_BLOCK_SIZE;

    // Number of bitmap entries. One entry represents 64 blocks.
    size_t nb_max_bitmaps = nb_max_blocks / 64;

    state->dirty_blocks = calloc(nb_max_blocks, sizeof(*state->dirty_blocks));
    state->index_dirty_blocks = 0;

    state->dirty_bitmaps = calloc(nb_max_bitmaps,
                                  sizeof(*state->dirty_bitmaps));
    state->nb_max_dirty_bitmaps = nb_max_bitmaps;

    state->make_dirty = dirty_state_make_dirty;
    state->print      = dirty_state_print_blocks;

    return state;
}

void
dirty_state_destroy(dirty_state_t* dirty_state)
{
    free(dirty_state->dirty_blocks);
    free(dirty_state->dirty_bitmaps);
    free(dirty_state);
}

// TODO: Might delete
static void
mmu_set_permissions(mmu_t* mmu, size_t start_address, uint8_t permission,
                    size_t size)
{
    if (start_address + size >= mmu->memory_size) {
        fprintf(stderr, "[%s]Address is to high!\n", __func__);
        return;
    }
    if (start_address < 0) {
        fprintf(stderr, "[%s]Address can't be below 0!\n", __func__);
        return;
    }

    // Set the provided address to the specified permission
    // TODO: Remove this cast to unsigned char*
    memset((unsigned char*)mmu->permissions + start_address, permission, size);
}


// Allocate memory for emulator.
static size_t
mmu_allocate(mmu_t* mmu, size_t size)
{
    // 16-byte align the allocation to make it more cache friendly
    size_t aligned_size = (size + 0xf) & ~0xf;

    // Get base for new allocation
    size_t base = mmu->current_allocation;

    // Guest memory is already full
    if (base >= mmu->memory_size) {
        return 1;
    }

    // Check if new allocation runs the emulator out of memory
    if ((mmu->current_allocation + aligned_size) >= mmu->memory_size) {
        fprintf(stderr, "[%s] Emulator is out of memory!\n", __func__);
        return 1;
    }

    // Update current allocation size
    mmu->current_allocation += aligned_size;

    // Set permissions of newly allocated memory to unitialized and writeable.
    // Keep extra memory added by the alignment as uninitialized.
    mmu->set_permissions(mmu, base, PERM_RAW | PERM_WRITE, size);

    return 0;
}


// Guest write function. Function does intentionally not checked for writes
// outside of the guest allocated memory or memory given to the emulator
//
// TODO: Should we check for write outside of allocated memory?
static void
mmu_write(mmu_t* mmu, size_t destination_address, uint8_t* source_buffer, size_t size)
{
    // Check permission of memory we are about to write to. If any of the addresses has the PERM_READ_AFTER_WRITE bit
    // set, we will remove it from all of them. If none of the addresses has it set, we will skip it.
    //
    // If any of the addresses we are about to write to is not writeable, return NULL.
    bool has_read_after_write = false;
    for (int i = 0; i < size; i++) {
        // If the RAW bit is set
        size_t current_address = destination_address + i;
        uint8_t current_perm = mmu->permissions[current_address];

        if ((current_perm & PERM_RAW) !=0) {
            has_read_after_write = true;
        }

        // If write permission is not set
        if ((current_perm & PERM_WRITE) == 0) {
            fprintf(stderr, "[%s] Error! Address 0x%lx not writeable. Has perm ",
                    __func__, current_address);
            print_permissions(current_perm);
            printf("\n");
            return;
        }
    }

    // Write the data
    printf("[%s] Writing 0x%lx bytes to address 0x%lx\n", __func__, size, destination_address);
    memcpy(mmu->memory + destination_address, source_buffer, size);

    // Mark blocks corresponding to addresses written to as dirty
    size_t start_block = destination_address / DIRTY_BLOCK_SIZE;
    size_t end_block   = (destination_address + size) / DIRTY_BLOCK_SIZE;
    for (size_t i = start_block; i <= end_block; i++) {
        mmu->dirty_state->make_dirty(mmu->dirty_state, i);
    }

    // Set permission of all memory written to readable.
    if (has_read_after_write) {
        for (int i = 0; i < size; i++) {
            // Remove the RAW bit TODO: Find out if this really is needed, we
            // might gain performance by removing it
            *(mmu->permissions + destination_address + i) &= ~PERM_READ;

            // Set permission of written memory to readable.
            *(mmu->permissions + destination_address + i) |= PERM_READ;
        }
    }
}


// Guest read function. Function is intentionally not bounds checked to allow for illegal reads which will be detected
// and recorded as a crash.
static void
mmu_read(mmu_t* mmu, uint8_t* destination_buffer, size_t source_address, size_t size)
{
    // If permission denied
    for (int i = 0; i < size; i++) {
        if ((*(mmu->permissions + source_address + i) & PERM_READ) == 0) {
            return;
        }
    }
    memcpy(destination_buffer, mmu->memory + source_address, size);
}

mmu_t*
mmu_create(size_t memory_size)
{
    const size_t base_allocation_address = 0x0;
    if (memory_size <= base_allocation_address) {
        fprintf(stderr, "[%s]Emulator needs more memory!\n", __func__);
        return NULL;
    }

    mmu_t* mmu = calloc(1, sizeof(mmu_t));
    if (!mmu) {
        return NULL;
    }

    mmu->memory_size        = memory_size;
    mmu->memory             = calloc(mmu->memory_size, sizeof(uint8_t));
    mmu->permissions        = calloc(mmu->memory_size, sizeof(uint8_t));
    mmu->current_allocation = base_allocation_address;
    if (!mmu->memory || !mmu->permissions) {
        return NULL;
    }

    mmu->allocate        = mmu_allocate;
    mmu->set_permissions = mmu_set_permissions;
    mmu->write           = mmu_write;
    mmu->read            = mmu_read;

    mmu->dirty_state     = dirty_state_create(memory_size);

    return mmu;
}

void
mmu_destroy(mmu_t* mmu)
{
    if (mmu->dirty_state) {
        dirty_state_destroy(mmu->dirty_state);
    }
    if (mmu) {
        free(mmu->memory);
        free(mmu->permissions);
        free(mmu);
    }
    return;
}
