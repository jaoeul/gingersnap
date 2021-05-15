#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "risc_v_emu.h"

#include "../shared/endianess_converter.h"
#include "../shared/logger.h"
#include "../shared/print_utils.h"

/* ----------------------- Instruction meta functions ------------------------*/

static uint8_t
utype_get_rd(uint32_t instruction)
{
    return (instruction >> 7) & 0x1f;
}

static void
set_register(risc_v_emu_t* emu, uint8_t reg, uint32_t value)
{
    emu->registers[reg] = value;
    ginger_log(INFO, "Set register: %u to 0x%x\n", reg, emu->registers[reg]);
}

/* --------------------- End instruction meta functions ----------------------*/

/* ------------------------------ Instructions -------------------------------*/

static void
auipc(risc_v_emu_t* emu, uint32_t instruction)
{
    uint32_t result = emu->registers[REG_PC] + (instruction & 0xfffff000);
    set_register(emu, utype_get_rd(instruction), result);
}

/* ---------------------------- End instructions -----------------------------*/

// Execute the instruction which the pc is pointing to.
static void
risc_v_emu_execute_next_instruction(risc_v_emu_t* emu)
{
    // 64bit risc v uses 32bit instructions
    uint8_t instruction_bytes[4] = {0};
    for (int i = 0; i < 4; i++) {
        // Check for execute permission
        const uint8_t current_permission = emu->mmu->permissions[emu->registers[REG_PC] + i];
        if ((current_permission & PERM_EXEC) == 0) {
            ginger_log(ERROR, "No exec perm set on address: 0x%x\n", emu->registers[REG_PC] + i);
            abort();
        }
        instruction_bytes[i] = emu->mmu->memory[emu->registers[REG_PC] + i];
    }

    // Get the opcode which the program counter currently points to.
    // Risc v 64i instructions have the opcode encoded in the first 6 bits of
    // the instruction.
    const uint32_t current_instruction = (uint32_t)byte_arr_to_u64(instruction_bytes, 4, LSB);
    const uint8_t  current_opcode      = current_instruction & 127; // & 0b1111111

    ginger_log(INFO, "Current instruction: 0x%x\n", current_instruction);
    ginger_log(INFO, "Current opcode: 0x%x\n", current_opcode);

    // Execute the instruction
    emu->instructions[current_opcode](emu, current_instruction);
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
        ginger_log(ERROR, "[%s]Source and destination emulators differ in memory size!\n", __func__);
        return false;
    }
}

static void
risc_v_emu_stack_push(risc_v_emu_t* emu, uint8_t bytes[], size_t nb_bytes)
{
    emu->mmu->write(emu->mmu, emu->registers[REG_SP] - nb_bytes, bytes, nb_bytes);
    emu->registers[REG_SP] -= nb_bytes;
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
        ginger_log(ERROR, "[%s]Could not create emu!\n", __func__);
        return NULL;
    }
    emu->mmu = mmu_create(memory_size);

    if (!emu->mmu) {
        ginger_log(ERROR, "[%s]Could not create mmu!\n", __func__);
        risc_v_emu_destroy(emu);
        return NULL;
    }

    // Set the pubilcly accessible function pointers
    emu->execute    = risc_v_emu_execute_next_instruction;
    emu->fork       = risc_v_emu_fork;
    emu->stack_push = risc_v_emu_stack_push;
    emu->destroy    = risc_v_emu_destroy;

    emu->instructions[0x17] = auipc;

    return emu;
}
