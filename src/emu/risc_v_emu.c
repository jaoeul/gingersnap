#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "../shared/print_utils.h"
#include "risc_v_emu.h"

// Initialize the Risc V emulator.
static void
risc_v_emu_init(risc_v_emu_t* emu)
{
    return;
}

// Execute the instruction which the pc is pointing to.
static void
risc_v_emu_execute(risc_v_emu_t* emu)
{
    return;
}

// Reset state of emulator to that of a another emulator.
static bool
risc_v_emu_fork(risc_v_emu_t* destination_emu, risc_v_emu_t* source_emu)
{
    // Fork register state
    memcpy(&destination_emu->registers, &source_emu->registers, sizeof(destination_emu->registers));

    // Fork memory state
    if (destination_emu->mmu->memory_size == source_emu->mmu->memory_size) {
        memcpy(destination_emu->mmu->memory, source_emu->mmu->memory, destination_emu->mmu->memory_size);
        return true;
    }
    else {
        fprintf(stderr, "[%s]Source and destination emulators differ in memory size!\n", __func__);
        return false;
    }
}

static void
risc_v_emu_stack_push(risc_v_emu_t* emu, uint8_t bytes[], size_t nb_bytes)
{
    emu->mmu->write(emu->mmu, emu->registers.sp - nb_bytes, bytes, nb_bytes);
    emu->registers.sp -= nb_bytes;
}

// Free the memory allocated for an emulator.
static void
risc_v_emu_destroy(risc_v_emu_t* emu)
{
    if (emu) {
        mmu_destroy(emu->mmu);
        free(emu);
    }
    return;
}

// Allocate memory and assign the emulators function pointers to correct values.
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
    emu->init       = risc_v_emu_init;
    emu->execute    = risc_v_emu_execute;
    emu->fork       = risc_v_emu_fork;
    emu->stack_push = risc_v_emu_stack_push;
    emu->destroy    = risc_v_emu_destroy;

    return emu;
}
