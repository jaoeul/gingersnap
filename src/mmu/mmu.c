/**
 * Guest memory layout:
 *
 * ===============================================================
 * |  Program headers  | <-- Guest stack (1MiB) | Guest heap --> |
 * ===============================================================
 * ^                                            ^                ^
 * |                                            |                |
 * Address 0.                                   Initial stack pointer (grows downwards).
 *                                              |                |
 *                                              Initial curr_alloc_adr (grows upwards).
 *                                                               |
 *                                                               Address: mmu->memory_size.
 *
 * Note that instead of having the stack and the heap traditianally growing towards eachother,
 * this emulator and mmu implements them differently. This is to avoid supporting emulation of big
 * allocations, which would use the mmap syscall instead of brk/sbrk and to safely allow for
 * allocatons of big chunks of memory on the heap without overwriting the stack. It will however
 * lead to diffing values returned by the brk/sbrk syscall, but this should hopefully not impact
 * program execution.
 */


#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mmu.h"

#include "../shared/endianess_converter.h"
#include "../shared/logger.h"
#include "../shared/print_utils.h"
#include "../shared/vector.h"

// Print value of memory with corresponding permissions of an emulator.
static void
mmu_print_mem(mmu_t* mmu, size_t start_adr, const size_t range,
              const char size_letter)
{
    uint8_t data_size = 0;
    if (size_letter == 'b')      data_size = BYTE_SIZE;
    else if (size_letter == 'h') data_size = HALFWORD_SIZE;
    else if (size_letter == 'w') data_size = WORD_SIZE;
    else if (size_letter == 'g') data_size = GIANT_SIZE;
    else { ginger_log(ERROR, "Invalid size letter!\n"); return; }
    printf("\n");
    for (size_t i = start_adr; i < start_adr + (range * data_size); i += data_size) {
        printf("0x%lx\t", i);
        printf("Value: 0x%.*lx\t", data_size * 2, byte_arr_to_u64(&mmu->memory[i], data_size, LSB));
        printf("Perm: ");
        print_permissions(mmu->permissions[i]);
        printf("\t");
        // Calculate if address is in a dirty block
        const size_t block       = i / DIRTY_BLOCK_SIZE;
        const size_t index       = block / 64; // 64 = number of bits in bitmap entry
        const size_t bit         = block % 64;
        const uint64_t shift_bit = 1;
        if ((mmu->dirty_state->dirty_bitmap[index] & (shift_bit << bit)) == 0) {
            printf("Block clean\n");
        }
        else {
            printf("Block dirty\n");
        }
    }
}

__attribute__((used))
static void
print_emu_memory_allocated(mmu_t* mmu)
{
    for (size_t i = 0; i < mmu->curr_alloc_adr - 1; i++) {
        printf("Address: 0x%lx\t", i);
        printf("Value: 0x%x\t", mmu->memory[i]);
        printf("Perm: ");
        print_permissions(mmu->permissions[i]);
        printf("\t");
        printf("In dirty block: ");

        // Calculate if address is in a dirty block
        const size_t block       = i / DIRTY_BLOCK_SIZE;
        const size_t index       = block / 64; // 64 = number of bits in bitmap entry
        const size_t bit         = block % 64;
        const uint64_t shift_bit = 1;

        if ((mmu->dirty_state->dirty_bitmap[index] & (shift_bit << bit)) == 0) {
            printf("NO");
        }
        else {
            printf("YES");
        }
        printf("\n");
    }
}

static void
dirty_state_print_blocks(dirty_state_t* state)
{
    for (size_t i = 0; i < state->nb_dirty_blocks; i++) {
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

    // If block is not already dirty.
    if ((state->dirty_bitmap[index] & (shift_bit << bit)) == 0) {
        // Append the block index of the dirty block.
        state->dirty_blocks[state->nb_dirty_blocks] = block;
        state->nb_dirty_blocks++;

        // Mark block as dirty.
        state->dirty_bitmap[index] |= 1 << bit;
    }

    return;
}

static void
dirty_state_clear(dirty_state_t* state)
{
    memset(state->dirty_blocks, 0, state->nb_dirty_blocks * sizeof(state->dirty_blocks[0]));
    state->nb_dirty_blocks = 0;
}

// mmu:        The mmu.
// start_adr:  Offset in the emulators memory to the address where permissions will be set.
// permission: uint8_t representation of the permission to write.
// size:       The amount of bytes of which the specified permission will be set.
static void
mmu_set_permissions(mmu_t* mmu, size_t start_adr, uint8_t permission, size_t size)
{
    if (start_adr + size >= mmu->memory_size) {
        ginger_log(ERROR, "[%s] Address is to high!\n", __func__);
        return;
    }

    // Set the provided address to the specified permission
    // TODO: Remove this cast to unsigned char*
    memset((unsigned char*)mmu->permissions + start_adr, permission, size);
}

// Allocate memory for emulator. Returns the virtual guest address of the allocated memory.
static size_t
mmu_allocate(mmu_t* mmu, size_t size, uint8_t* error)
{
    // 16-byte align the allocation to make it more cache friendly.
    size_t aligned_size = (size + 0xf) & ~0xf;

    // Guest memory is already full.
    if (mmu->curr_alloc_adr >= mmu->memory_size) {
        ginger_log(ERROR, "[%s] Error! Emulator memory already full!\n", __func__);
        *error = MMU_ALLOC_ERROR_MEM_FULL;
        return 0;
    }

    // Check if new allocation runs the emulator out of memory.
    if (mmu->curr_alloc_adr + aligned_size >= mmu->memory_size) {
        ginger_log(ERROR, "[%s] Emulator is out of memory!\n", __func__);
        *error = MMU_ALLOC_ERROR_WOULD_OVERRUN;
        return 0;
    }

    // We want to return the virtual address of the allocation, so save it.
    const size_t base = mmu->curr_alloc_adr;

    // Update current allocation size.
    mmu->curr_alloc_adr += aligned_size;

    // Set permissions of newly allocated memory to unitialized and writeable.
    // Keep extra memory added by the alignment as uninitialized.
    mmu->set_permissions(mmu, base, MMU_PERM_RAW | MMU_PERM_WRITE, aligned_size);

    *error = MMU_ALLOC_NO_ERROR;
    return base;
}

// TODO: Should we check for write outside of allocated memory?
static mmu_write_error_t
mmu_write(mmu_t* mmu, size_t dst_adr, const uint8_t* src_buffer, size_t size)
{
    if (dst_adr + size > mmu->memory_size) {
        ginger_log(WARNING, "[%s] Write outside of total emulator memory!\n", __func__);
        return MMU_WRITE_ERROR_ADR_OUT_OF_RANGE;
    }

    // Check permission of memory we are about to write to. If any of the addresses has the MMU_PERM_READ_AFTER_WRITE bit
    // set, we will remove it from all of them. If none of the addresses has it set, we will skip it.
    //
    // If any of the addresses we are about to write to is not writeable, return NULL.
    bool has_read_after_write = false;
    for (int i = 0; i < size; i++) {

        // Offset to the dst address from start of emulator memory.
        const size_t curr_adr = dst_adr + i;
        const uint8_t curr_perm = mmu->permissions[curr_adr];

        // If the RAW bit is set
        if ((curr_perm & MMU_PERM_RAW) !=0) {
            has_read_after_write = true;
        }

        // If write permission is not set
        if ((curr_perm & MMU_PERM_WRITE) == 0) {
            ginger_log(ERROR, "[%s] Address 0x%lx not writeable. Has perm ", __func__, curr_adr);
            print_permissions(curr_perm);
            printf("\n");
            return MMU_WRITE_ERROR_NO_PERM;
        }
    }

    // Write the data
    ginger_log(DEBUG, "[%s] Writing 0x%lx bytes to address 0x%lx\n", __func__, size, dst_adr);
    memcpy(mmu->memory + dst_adr, src_buffer, size);

    // Mark blocks corresponding to addresses written to as dirty
    size_t start_block = dst_adr / DIRTY_BLOCK_SIZE;
    size_t end_block   = (dst_adr + size) / DIRTY_BLOCK_SIZE;
    for (size_t i = start_block; i <= end_block; i++) {
        mmu->dirty_state->make_dirty(mmu->dirty_state, i);
    }

    // Set permission of all memory written to readable.
    if (has_read_after_write) {
        for (int i = 0; i < size; i++) {
            // Remove the RAW bit TODO: Find out if this really is needed, we
            // might gain performance by removing it
            *(mmu->permissions + dst_adr + i) &= ~MMU_PERM_RAW;

            // Set permission of written memory to readable.
            *(mmu->permissions + dst_adr + i) |= MMU_PERM_READ;
        }
    }
    return MMU_WRITE_NO_ERROR;
}

// Read from guest memory into buffer. Function is intentionally not bounds
// checked to allow for illegal reads which will be detected and recorded as a
// crash.
static uint8_t
mmu_read(mmu_t* mmu, uint8_t* dst_buffer, const uint64_t src_adr, size_t size)
{
    // We could enable the user to crash as soon as invalid data is accessed. However, not doing it
    // allows us to see where the execution goes even after an invalid read.
    if (src_adr + size > mmu->memory_size) {
        ginger_log(WARNING, "Address 0x%lx is outside of emulator total memory!\n", src_adr + size);
        return MMU_READ_ERROR_ADR_OUT_OF_RANGE;
    }

    // If permission denied
    for (int i = 0; i < size; i++) {
        if ((*(mmu->permissions + src_adr + i) & MMU_PERM_READ) == 0) {
            ginger_log(DEBUG, "Illegal read at address: 0x%lx\n", src_adr + i);
            return MMU_READ_ERROR_NO_PERM;
        }
    }
    memcpy(dst_buffer, mmu->memory + src_adr, size);
    return MMU_READ_NO_ERROR;
}

// Search for specified value in guest memory. The size of the value is
// specififed by `size_letter`. Valid size letters are b(byte), h(halfword),
// w(word) or g(giant). If matching values are found, store their guest memory
// addresses in a vector and return it. Otherwise, return NULL.
static vector_t*
mmu_search(mmu_t* mmu, const uint64_t needle, const char size_letter)
{
    uint8_t data_size = 0;
    vector_t* hits    = vector_create(sizeof(size_t));

    if (size_letter == 'b')      data_size = BYTE_SIZE;
    else if (size_letter == 'h') data_size = HALFWORD_SIZE;
    else if (size_letter == 'w') data_size = WORD_SIZE;
    else if (size_letter == 'g') data_size = GIANT_SIZE;
    else { ginger_log(ERROR, "Invalid size letter!\n"); return false; }

    for (size_t i = 0; i < mmu->memory_size; i += data_size) {
        uint64_t curr_value = byte_arr_to_u64(&mmu->memory[i], data_size, LSB);
        if (curr_value == needle) {
            vector_append(hits, &i);
        }
    }

    if (vector_length(hits) > 0) {
        return hits;
    } else {
        vector_destroy(hits);
        return NULL;
    }
}

void
print_permissions(uint8_t perms)
{
    if (perms == 0) {
        printf("None");
        return;
    }
    if ((perms & (1 << 0))) {
        printf("E ");
    }
    if ((perms & (1 << 1))) {
        printf("W ");
    }
    if ((perms & (1 << 2))) {
        printf("R ");
    }
    if ((perms & (1 << 3))) {
        printf("RAW");
    }
}

dirty_state_t*
dirty_state_create(size_t memory_size)
{
    dirty_state_t* state = calloc(1, sizeof(*state));

    // Max possible number of dirty memory blocks. This is capped to the total
    // memory size / DIRTY_BLOCK_SIZE since we will not allow duplicates of
    // dirtied blocks in the dirty_state->dirty_blocks vector, and will
    // therefore never need more than this number of entries.
    size_t nb_max_blocks = memory_size / DIRTY_BLOCK_SIZE;

    // Number of bitmap entries. One entry represents 64 blocks.
    size_t nb_max_bitmaps = nb_max_blocks / 64;

    state->dirty_blocks = calloc(nb_max_blocks, sizeof(*state->dirty_blocks));
    state->nb_dirty_blocks = 0;

    state->dirty_bitmap = calloc(nb_max_bitmaps, sizeof(*state->dirty_bitmap));
    state->nb_max_dirty_bitmaps = nb_max_bitmaps;

    state->make_dirty = dirty_state_make_dirty;
    state->print      = dirty_state_print_blocks;
    state->clear      = dirty_state_clear;

    return state;
}

void
dirty_state_destroy(dirty_state_t* dirty_state)
{
    free(dirty_state->dirty_blocks);
    free(dirty_state->dirty_bitmap);
    free(dirty_state);
}

mmu_t*
mmu_create(const size_t memory_size, const size_t base_alloc_adr)
{
    mmu_t* mmu = calloc(1, sizeof(mmu_t));
    if (!mmu) {
        return NULL;
    }

    mmu->memory_size        = memory_size;
    mmu->memory             = calloc(mmu->memory_size, sizeof(uint8_t));
    mmu->permissions        = calloc(mmu->memory_size, sizeof(uint8_t));
    mmu->dirty_state        = dirty_state_create(memory_size);

    if (!mmu->memory || !mmu->permissions || !mmu->dirty_state) {
        ginger_log(ERROR, "[%s:%u] Out of memory!\n", __func__, __LINE__);
        abort();
    }

    // The base allocation address needs to be higher than the program brk address and the
    // size of the stack. The elf loader makes sure that we do not overwrite guest allocated
    // memory when loading the elf. And by setting the initial curr_alloc_adr to the address
    // of where the stack starts, we make sure that the stack does not overwrite guest allocated
    // memory, since it grows downwards.
    //
    //
    mmu->curr_alloc_adr = base_alloc_adr;

    // API functions.
    mmu->allocate        = mmu_allocate;
    mmu->set_permissions = mmu_set_permissions;
    mmu->write           = mmu_write;
    mmu->read            = mmu_read;
    mmu->search          = mmu_search;
    mmu->print           = mmu_print_mem;

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
