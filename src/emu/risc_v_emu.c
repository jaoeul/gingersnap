#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "risc_v_emu.h"

static void*
mmu_set_permissions(mmu_t* mmu, size_t start_address, permission_t permission, size_t size)
{
    if (start_address + size >= mmu->memory_size) {
        fprintf(stderr, "[%s]Address is to high!\n", __func__);
    }

    // Set the provided address to the specified permission
    // TODO: Remove this cast to unsigned char*
    return memset((unsigned char*)mmu->permissions + start_address, permission, size);
}

/**
 * Allocate memory for emulator.
 */
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
    mmu->set_permissions(mmu, base, PERM_READ_AFTER_WRITE | PERM_WRITE, size);

    return 0;
}

/**
 * Guest write function. Function is intentionally not bounds checked to allow for illegal writes which will be detected
 * and recorded as a crash.
 *
 * TODO: Should we check for write outside of allocated memory?
 */
static void*
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
        permission_t current_perm = mmu->permissions[current_address];

        if ((current_perm & PERM_READ_AFTER_WRITE) !=0) {
            has_read_after_write = true;
        }

        // If write permission is not set
        if ((*(mmu->permissions + destination_address + i) & PERM_WRITE) == 0) {
            return NULL;
        }
    }

    // Set permission of all memory written to readable.
    if (has_read_after_write) {
        for (int i = 0; i < size; i++) {
            // Remove the RAW bit
            *(mmu->permissions + destination_address + i) &= ~PERM_READ_AFTER_WRITE;

            // Set permission of written memory to readable.
            *(mmu->permissions + destination_address + i) |= PERM_READ;
        }
    }

    // Write the data
    return memcpy(mmu->memory + destination_address, source_buffer, size);
}

/**
 * Guest read function. Function is intentionally not bounds checked to allow for illegal reads which will be detected
 * and recorded as a crash.
 */
static void*
mmu_read(mmu_t* mmu, uint8_t* destination_buffer, size_t source_address, size_t size)
{
    // If permission denied
    for (int i = 0; i < size; i++) {
        if ((*(mmu->permissions + source_address + i) & PERM_READ) == 0) {
            return NULL;
        }
    }

    return memcpy(destination_buffer, mmu->memory + source_address, size);
}

static mmu_t*
mmu_create(size_t memory_size)
{
    const size_t base_allocation_address = 0x10000;
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

    return mmu;
}

static void
destroy_mmu(mmu_t* mmu)
{
    if (mmu) {
        free(mmu->memory);
        free(mmu->permissions);
        free(mmu);
    }
    return;
}

/**
 * Initialize the Risc V emulator.
 */
static void
risc_v_emu_init(risc_v_emu_t* emu)
{

    return;
}

/**
 * Execute the instruction which the pc is pointing to.
 */
static void
risc_v_emu_execute(risc_v_emu_t* emu)
{

    return;
}

/**
 * Reset state of emulator to that of a another emulator.
 */
static bool
risc_v_emu_reset(risc_v_emu_t* destination_emu, risc_v_emu_t* source_emu)
{
    // Reset register state
    memcpy(&destination_emu->registers, &source_emu->registers, sizeof(destination_emu->registers));

    // Reset memory state
    if (destination_emu->mmu->memory_size == source_emu->mmu->memory_size) {
        memcpy(destination_emu->mmu->memory, source_emu->mmu->memory, destination_emu->mmu->memory_size);
        return true;
    }
    else {
        fprintf(stderr, "[%s]Source and destination emulators differ in memory size!\n", __func__);
        return false;
    }
}

/**
 * Free the memory allocated for an emulator.
 */
static void
risc_v_emu_destroy(risc_v_emu_t* emu)
{
    if (emu) {
        destroy_mmu(emu->mmu);
        free(emu);
    }
    return;
}

/**
 * Allocate memory and assign the emulators function pointers to correct values.
 */
risc_v_emu_t*
risc_v_emu_create(size_t memory_size)
{
    risc_v_emu_t* emu = calloc(1, sizeof(risc_v_emu_t));
    if (!emu) {
        fprintf(stderr, "[%s]Could not create emu!\n", __func__);
        return NULL;
    }
    emu->mmu = mmu_create(memory_size);

    if (!emu->mmu) {
        fprintf(stderr, "[%s]Could not create mmu!\n", __func__);
        risc_v_emu_destroy(emu);
        return NULL;
    }

    // Set the pubilcly accessible function pointers

    emu->init    = risc_v_emu_init;
    emu->execute = risc_v_emu_execute;
    emu->reset   = risc_v_emu_reset;
    emu->destroy = risc_v_emu_destroy;

    return emu;
}
