#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "risc_v_emu.h"

#include "../shared/endianess_converter.h"
#include "../shared/logger.h"
#include "../shared/print_utils.h"

// TODO: Remove globals.
uint64_t nb_executed_instructions = 0;

// Risc V 32i + 64i Instructions and corresponding opcode.
enum {
    LUI                              = 0x37,
    AUIPC                            = 0x17,
    JAL                              = 0x6f,
    JALR                             = 0x67,
    BRANCH                           = 0x63,
    LOAD                             = 0x04,
    STORE                            = 0x23,
    ARITHMETIC_I_TYPE                = 0x13,
    ARITHMETIC_R_TYPE                = 0x33,
    FENCE                            = 0x0f,
    ENV                              = 0x73,
    ARITHMETIC_64_REGISTER_IMMEDIATE = 0x1b,
    ARITHMETIC_64_REGISTER_REGISTER  = 0x3b,
};

/* ========================================================================== */
/*                         Instruction meta functions                         */
/* ========================================================================== */

static int32_t
i_type_get_immediate(const uint32_t instruction)
{
    return (instruction >> 20) & 0xfff; // 0b111111111111
}

__attribute__((used))
static int32_t
s_type_get_immediate(const uint32_t instruction)
{
    const uint32_t immediate40  = (instruction >> 7)  & 0x1f;
    const uint32_t immediate115 = (instruction >> 25) & 0x7f; 

    uint32_t target_immediate = (immediate115  << 5) | immediate40;
    target_immediate = (target_immediate << 20) >> 20;

    return target_immediate;
}

static int32_t
b_type_get_immediate(const uint32_t instruction)
{
    const uint32_t immediate11  = (instruction >> 7)  & 0x01;
    const uint32_t immediate41  = (instruction >> 8)  & 0x0f;
    const uint32_t immediate105 = (instruction >> 25) & 0x3f;
    const uint32_t immediate12  = (instruction >> 31) & 0x01;

    uint32_t target_immediate = (immediate12  << 12) |
                                (immediate11  << 11) |
                                (immediate105 << 5)  |
                                (immediate41  << 1);
    target_immediate = (target_immediate << 19) >> 19;

    return target_immediate;
}

static uint32_t
j_type_get_immediate(const uint32_t instruction)
{
    return (instruction >> 12) & 0xfffff;
}

static uint32_t
get_funct7(const uint32_t instruction)
{
    return (instruction >> 25) & 0x7f;
}

static uint32_t
get_rs2(const uint32_t instruction)
{
    return (instruction >> 20) & 0x1f;
}

static uint32_t
get_rs1(const uint32_t instruction)
{
    return (instruction >> 15) & 0x1f;
}

static uint32_t
get_funct3(const uint32_t instruction)
{
    return (instruction >> 12) & 0x07;
}

static uint32_t
get_rd(const uint32_t instruction)
{
    return (instruction >> 7) & 0x1f;
}

static uint8_t
get_opcode(const uint32_t instruction)
{
    return instruction & 0x7f;
}

static uint32_t
get_next_instruction(const risc_v_emu_t* emu)
{
    uint8_t instruction_bytes[4] = {0};
    for (int i = 0; i < 4; i++) {
        const uint8_t current_permission = emu->mmu->permissions[emu->registers[REG_PC] + i];
        if ((current_permission & PERM_EXEC) == 0) {
            ginger_log(ERROR, "No exec perm set on address: 0x%x\n", emu->registers[REG_PC] + i);
            abort();
        }
        instruction_bytes[i] = emu->mmu->memory[emu->registers[REG_PC] + i];
    }
    return (uint32_t)byte_arr_to_u64(instruction_bytes, 4, LSB);
}

static void
set_register(risc_v_emu_t* emu, const uint8_t reg, const uint32_t value)
{
    emu->registers[reg] = value;
    ginger_log(INFO, "Set register: %u to 0x%x\n", reg, emu->registers[reg]);
}

static uint32_t
get_register(const risc_v_emu_t* emu, const uint8_t reg)
{
    return emu->registers[reg];
}

static uint32_t
get_pc(const risc_v_emu_t* emu)
{
    return get_register(emu, REG_PC);
}

static void
set_pc(risc_v_emu_t* emu, const uint32_t value)
{
    set_register(emu, REG_PC, value);
}

static void
increment_pc(risc_v_emu_t* emu)
{
    set_pc(emu, get_pc(emu) + 4);
}

static void
set_rd(risc_v_emu_t* emu, const uint32_t instruction, const uint32_t value)
{
    set_register(emu, get_rd(instruction), value);
}


/* ========================================================================== */
/*                           Instruction functions                            */
/* ========================================================================== */

/* --------------------------- U-Type instructions ---------------------------*/

static void
lui(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t result = instruction & 0xfffff000;
    set_register(emu, get_rd(instruction), result);
    increment_pc(emu);
}

// Add upper immediate to pc.
static void
auipc(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t result = emu->registers[REG_PC] + (instruction & 0xfffff000);
    set_register(emu, get_rd(instruction), result);
    increment_pc(emu);
}

/* --------------------------- J-Type instructions ---------------------------*/

static void
jal(risc_v_emu_t* emu, const uint32_t instruction)
{
    // When an unsigned int and an int are added together, the int is first
    // converted to unsigned int before the addition takes place. This makes
    // the following addition work.
    const int32_t jump_offset = (int32_t)j_type_get_immediate(instruction);
    const uint32_t result     = get_register(emu, REG_PC) + jump_offset;
    set_register(emu, REG_PC, result);

    const uint32_t return_address = get_register(emu, REG_PC) + 4;
    set_register(emu, get_rd(instruction), return_address);

    // TODO: Make use of following if statement.
    //
    // Jump is unconditional jump.
    if (get_rd(instruction) == REG_ZERO) {
        return;
    }
    // Jump is function call.
    else {
        return;
    }
}

/* --------------------------- I-Type instructions ---------------------------*/

static void
jalr(risc_v_emu_t* emu, const uint32_t instruction)
{
    // Calculate target jump address.
    const int32_t  immediate = i_type_get_immediate(instruction);
    const uint32_t rs1       = get_rs1(instruction);
    const uint32_t target    = (immediate + rs1) & ~1;

    // Save pc + 4 into register rd.
    set_register(emu, get_rd(instruction), get_register(emu, REG_PC + 4));

    // Jump to target address.
    set_pc(emu, target);
}

static void
lb(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t target   = get_rs1(instruction) + i_type_get_immediate(instruction);

    // Read 1 byte from target guest address into buffer.
    uint8_t loaded_bytes[1] = {0};
    emu->mmu->read(emu->mmu, loaded_bytes, target, 1);

    int32_t loaded_value = (int32_t)byte_arr_to_u64(loaded_bytes, 1, LSB);

    set_rd(emu, instruction, loaded_value);
    increment_pc(emu);
}

static void
lh(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t target   = get_rs1(instruction) + i_type_get_immediate(instruction);

    // Read 2 bytes from target guest address into buffer.
    uint8_t loaded_bytes[2] = {0};
    emu->mmu->read(emu->mmu, loaded_bytes, target, 2);

    // Sign-extend.
    int32_t loaded_value = (int32_t)(uint32_t)byte_arr_to_u64(loaded_bytes, 2, LSB);

    set_rd(emu, instruction, loaded_value);
    increment_pc(emu);
}

static void
lw(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t target = get_rs1(instruction) + i_type_get_immediate(instruction);

    // Read 4 bytes from target guest address into buffer.
    uint8_t loaded_bytes[4] = {0};
    emu->mmu->read(emu->mmu, loaded_bytes, target, 4);
    uint32_t loaded_value = (uint32_t)byte_arr_to_u64(loaded_bytes, 4, LSB);

    set_rd(emu, instruction, loaded_value);
    increment_pc(emu);
}

static void
lbu(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t target   = get_rs1(instruction) + i_type_get_immediate(instruction);

    // Read 1 byte from target guest address into buffer.
    uint8_t loaded_bytes[1] = {0};
    emu->mmu->read(emu->mmu, loaded_bytes, target, 1);

    uint32_t loaded_value = (uint32_t)byte_arr_to_u64(loaded_bytes, 1, LSB);

    set_rd(emu, instruction, loaded_value);
    increment_pc(emu);
}

static void
lhu(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t target = get_rs1(instruction) + i_type_get_immediate(instruction);

    // Read 2 bytes from target guest address into buffer.
    uint8_t loaded_bytes[2] = {0};
    emu->mmu->read(emu->mmu, loaded_bytes, target, 2);

    // Zero-extend to 32 bit.
    uint32_t loaded_value = (uint32_t)byte_arr_to_u64(loaded_bytes, 2, LSB);

    set_rd(emu, instruction, loaded_value);
    increment_pc(emu);
}

static void
lwu(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t base   = get_rs1(instruction);
    const uint32_t offset = i_type_get_immediate(instruction);
    const uint32_t target = base + offset;

    uint8_t loaded_bytes[4] = {0};
    emu->mmu->read(emu->mmu, loaded_bytes, target, 4);
    const uint64_t result = (uint64_t)byte_arr_to_u64(loaded_bytes, 4, LSB);

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
ld(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t base   = get_rs1(instruction);
    const uint32_t offset = i_type_get_immediate(instruction);
    const uint32_t target = base + offset;

    uint8_t loaded_bytes[8] = {0};
    emu->mmu->read(emu->mmu, loaded_bytes, target, 8);
    const uint64_t result = byte_arr_to_u64(loaded_bytes, 8, LSB);

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
execute_load_instruction(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = get_funct3(instruction);

    if (funct3 == 0) {
        lb(emu, instruction);
    }
    else if (funct3 == 1) {
        lh(emu, instruction);
    }
    else if (funct3 == 2) {
        lw(emu, instruction);
    }
    else if (funct3 == 3) {
        ld(emu, instruction);
    }
    else if (funct3 == 4) {
        lbu(emu, instruction);
    }
    else if (funct3 == 5) {
        lhu(emu, instruction);
    }
    else if (funct3 == 6) {
        lwu(emu, instruction);
    }
    else {
        ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
        abort();
    }
}

static void
addi(risc_v_emu_t* emu, const uint32_t instruction)
{
    const int32_t  addend = i_type_get_immediate(instruction);
    const uint32_t result = get_rs1(instruction) + addend;
    set_rd(emu, instruction, result);
    increment_pc(emu);
}

// Set less than immediate.
static void
slti(risc_v_emu_t* emu, const uint32_t instruction)
{
    const int32_t compare = i_type_get_immediate(instruction);

    if (get_rs1(instruction) < compare) {
        set_rd(emu, instruction, 1);
    }
    else {
        set_rd(emu, instruction, 0);
    }
    increment_pc(emu);
}

// Set less than immediate.
static void
sltiu(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t immediate = (uint32_t)i_type_get_immediate(instruction);

    if (get_rs1(instruction) < immediate) {
        set_rd(emu, instruction, 1);
    }
    else {
        set_rd(emu, instruction, 0);
    }
    increment_pc(emu);
}

static void
xori(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t immediate = (uint32_t)i_type_get_immediate(instruction);
    const uint32_t result = get_rs1(instruction) ^ immediate;
    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
ori(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t immediate = (uint32_t)i_type_get_immediate(instruction);
    const uint32_t result = get_rs1(instruction) | immediate;
    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
andi(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t immediate = (uint32_t)i_type_get_immediate(instruction);
    const uint32_t result = get_rs1(instruction) & immediate;
    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
slli(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t shamt  = i_type_get_immediate(instruction) & 0x3f;
    const uint32_t result = get_rs1(instruction) << shamt;
    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
srli(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t shamt  = i_type_get_immediate(instruction) & 0x3f;
    const uint32_t result = get_rs1(instruction) >> shamt;
    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
srai(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t shamt  = i_type_get_immediate(instruction) & 0x3f;
    const uint32_t result = (uint64_t)((int64_t)get_rs1(instruction) >> shamt);
    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
execute_arithmetic_i_instruction(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = get_funct3(instruction);

    if (funct3 == 0) {
        addi(emu, instruction);
    }
    else if (funct3 == 1) {
        slli(emu, instruction);
    }
    else if (funct3 == 2) {
        slti(emu, instruction);
    }
    else if (funct3 == 3) {
        sltiu(emu, instruction);
    }
    else if (funct3 == 4) {
        xori(emu, instruction);
    }
    else if (funct3 == 5) {
        const uint32_t funct7 = get_funct7(instruction);
        if (funct7 == 0) {
            srli(emu, instruction);
        }
        else if (funct7 == 32) {
            srai(emu, instruction);
        }
        else {
            ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
            abort();
        }
    }
    else if (funct3 == 6) {
        ori(emu, instruction);
    }
    else if (funct3 == 7) {
        andi(emu, instruction);
    }
    else if (funct3 == 1) {
        slli(emu, instruction);
    }
}

static void
fence(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(ERROR, "FENCE instruction not implemented!\n");
    abort();
}

static void
ecall(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(ERROR, "ECALL instruction not implemented!\n");
    abort();
}

static void
ebreak(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(ERROR, "EBREAK instruction not implemented!\n");
    abort();
}

static void
execute_env_instructions(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct12 = i_type_get_immediate(instruction);

    if (funct12 == 0) {
        ecall(emu, instruction);
    }
    else if (funct12 == 1) {
        ebreak(emu, instruction);
    }
    increment_pc(emu);
}

static void
addiw(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t immediate = (uint32_t)i_type_get_immediate(instruction);
    const uint32_t rs1       = get_rs1(instruction);
    const uint32_t addend    = get_register(emu, rs1);

    // FIXME: Carefully monitor casting logic of following line.
    const int64_t result = (uint64_t)((int64_t)addend + immediate);
    set_register(emu, get_rd(instruction), result);
    increment_pc(emu);
}

static void
slliw(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t shamt = i_type_get_immediate(instruction) & 0x1f;
    const uint32_t result = get_rs1(instruction) << shamt;
    set_register(emu, get_rd(instruction), result);
    increment_pc(emu);
}

static void
srliw(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t shamt = i_type_get_immediate(instruction) & 0x1f;
    const uint32_t result = get_rs1(instruction) >> shamt;
    set_register(emu, get_rd(instruction), result);
    increment_pc(emu);
}

static void
sraiw(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t shamt = i_type_get_immediate(instruction) & 0x1f;
    const uint32_t result = get_rs1(instruction) >> shamt;
    set_register(emu, get_rd(instruction), (int32_t)result);
    increment_pc(emu);
}

static void
execute_arithmetic_64_register_immediate_instructions(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = get_funct3(instruction);
    const uint32_t funct7 = get_funct7(instruction);

    if (funct3 == 0) {
        addiw(emu, instruction);
    }
    else if (funct3 == 1) {
        slliw(emu, instruction);
    }
    else if (funct3 == 5) {
        if (funct7 == 0 ) {
            srliw(emu, instruction);
        }
        else if (funct7 == 32 ) {
            sraiw(emu, instruction);
        }
        else {
            ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
            abort();
        }
    }
    else {
        ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
        abort();
    }
}

static void
addw(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs1  = get_rs1(instruction);
    const uint32_t rs2  = get_rs2(instruction);
    const uint32_t src1 = get_register(emu, rs1);
    const uint32_t src2 = get_register(emu, rs2);

    // Close your eyes!
    const uint64_t result = (uint64_t)(int64_t)(int32_t)(src1 + src2);

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
subw(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs1  = get_rs1(instruction);
    const uint32_t rs2  = get_rs2(instruction);
    const uint32_t src1 = get_register(emu, rs1);
    const uint32_t src2 = get_register(emu, rs2);

    // Close your eyes!
    const uint64_t result = (uint64_t)(int64_t)(int32_t)(src1 - src2);

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
sllw(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs1   = get_rs1(instruction);
    const uint32_t src   = get_register(emu, rs1);
    const uint32_t shamt = get_rs2(instruction) & 0x1f;

    const uint64_t result = (uint64_t)(int64_t)(int32_t)(src << shamt);

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
srlw(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs1   = get_rs1(instruction);
    const uint32_t src   = get_register(emu, rs1);
    const uint32_t shamt = get_rs2(instruction) & 0x1f;

    const uint64_t result = (uint64_t)(int64_t)(int32_t)(src >> shamt);

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
sraw(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs1   = get_rs1(instruction);
    const uint32_t src   = get_register(emu, rs1);
    const uint32_t shamt = get_rs2(instruction) & 0x1f;

    const uint64_t result = (uint64_t)(int64_t)((int32_t)src >> shamt);

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
execute_arithmetic_64_register_register_instructions(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = get_funct3(instruction);
    const uint32_t funct7 = get_funct7(instruction);

    if (funct3 == 0) {
        if (funct7 == 0) {
            addw(emu, instruction);
        }
        else if (funct7 == 32) {
            subw(emu, instruction);
        }
        else {
            ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
            abort();
        }
    }
    else if (funct3 == 1) {
        sllw(emu, instruction);
    }
    else if (funct3 == 5) {
        if (funct7 == 0) {
            srlw(emu, instruction);
        }
        else if (funct7 == 32) {
            sraw(emu, instruction);
        }
        else {
            ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
            abort();
        }
    }
    else {
        ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
        abort();
    }
}

/* --------------------------- R-Type instructions ---------------------------*/

static void
add(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs1 = get_rs1(instruction);
    const uint32_t rs2 = get_rs2(instruction);
    const uint32_t result = rs1 + rs2;
    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
sub(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs1 = get_rs1(instruction);
    const uint32_t rs2 = get_rs2(instruction);
    const uint32_t result = rs1 - rs2;
    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
sll(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs1         = get_rs1(instruction);
    const uint32_t shift_value = get_rs2(instruction) & 0xf1;
    const uint32_t result      = rs1 << shift_value;
    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
slt(risc_v_emu_t* emu, const uint32_t instruction)
{
    const int32_t rs1 = (int32_t)get_rs1(instruction);
    const int32_t rs2 = (int32_t)get_rs2(instruction);

    if (rs1 < rs2) {
        set_rd(emu, instruction, 1);
    }
    else {
        set_rd(emu, instruction, 0);
    }
    increment_pc(emu);
}

static void
sltu(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs1 = get_rs1(instruction);
    const uint32_t rs2 = get_rs2(instruction);

    if (rs1 < rs2) {
        set_rd(emu, instruction, 1);
    }
    else {
        set_rd(emu, instruction, 0);
    }
    increment_pc(emu);
}

static void
xor(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs1 = get_rs1(instruction);
    const uint32_t rs2 = get_rs2(instruction);
    set_rd(emu, instruction, get_register(emu, rs1) ^ get_register(emu, rs2));
    increment_pc(emu);
}

static void
srl(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs1         = get_rs1(instruction);
    const uint32_t shift_value = get_rs2(instruction) & 0xf1;
    const uint32_t result      = rs1 >> shift_value;
    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
sra(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs1         = get_rs1(instruction);
    const uint32_t shift_value = get_rs2(instruction) & 0xf1;
    const uint32_t result      = (uint32_t)((int32_t)rs1 >> shift_value);
    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
or(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs1 = get_rs1(instruction);
    const uint32_t rs2 = get_rs2(instruction);
    set_rd(emu, instruction, get_register(emu, rs1) | get_register(emu, rs2));
    increment_pc(emu);
}

static void
and(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs1 = get_rs1(instruction);
    const uint32_t rs2 = get_rs2(instruction);
    set_rd(emu, instruction, get_register(emu, rs1) & get_register(emu, rs2));
    increment_pc(emu);
}

static void
execute_arithmetic_32_r_instruction(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = get_funct3(instruction);
    const uint32_t funct7 = get_funct7(instruction);

    if (funct3 == 0) {
        if (funct7 == 0) {
            add(emu, instruction);
        }
        else if (funct7 == 32) {
            sub(emu, instruction);
        }
        else {
            ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
            abort();
        }
    }
    else if (funct3 == 1) {
        sll(emu, instruction);
    }
    else if (funct3 == 2) {
        slt(emu, instruction);
    }
    else if (funct3 == 3) {
        sltu(emu, instruction);
    }
    else if (funct3 == 4) {
        xor(emu, instruction);
    }
    else if (funct3 == 5) {
        if (funct7 == 0) {
            srl(emu, instruction);
        }
        else if (funct7 == 32) {
            sra(emu, instruction);
        }
        else {
            ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
            abort();
        }
    }
    else if (funct3 == 6) {
        or(emu, instruction);
    }
    else if (funct3 == 7) {
        and(emu, instruction);
    }
    else {
        ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
        abort();
    }
}

/* --------------------------- S-Type instructions ---------------------------*/

static void
sb(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t target      = get_rs1(instruction) + i_type_get_immediate(instruction);
    const uint8_t  store_value = get_rs2(instruction) & 0xff;

    emu->mmu->write(emu->mmu, target, &store_value, 1);
    increment_pc(emu);
}

static void
sh(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t target      = get_rs1(instruction) + i_type_get_immediate(instruction);
    const uint32_t rs2         = get_rs2(instruction);
    const uint64_t store_value = get_register(emu, rs2) & 0xffff;

    // TODO: Update u64_to_byte_arr to be able to handle smaller integers,
    //       removing the need for 8 byte array here.
    uint8_t store_bytes[8] = {0};
    u64_to_byte_arr(store_value, store_bytes, LSB);

    // Write 2 bytes into guest memory at target address.
    emu->mmu->write(emu->mmu, target, store_bytes, 2);
    increment_pc(emu);
}

static void
sw(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t target      = get_rs1(instruction) + i_type_get_immediate(instruction);
    const uint32_t rs2         = get_rs2(instruction);
    const uint64_t store_value = get_register(emu, rs2) & 0xffffffff;

    uint8_t store_bytes[8] = {0};

    // TODO: Update u64_to_byte_arr to be able to handle smaller integers,
    //       removing the need for 8 byte array here.
    u64_to_byte_arr(store_value, store_bytes, LSB);

    // Write 4 bytes into guest memory at target address.
    emu->mmu->write(emu->mmu, target, store_bytes, 4);
    increment_pc(emu);
}

static void
sd(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t target      = get_rs1(instruction) + i_type_get_immediate(instruction);
    const uint32_t rs2         = get_rs2(instruction);
    const uint64_t store_value = get_register(emu, rs2) & 0xffffffffffffffff;

    uint8_t store_bytes[8] = {0};
    u64_to_byte_arr(store_value, store_bytes, LSB);

    // Write 8 bytes into guest memory at target address.
    emu->mmu->write(emu->mmu, target, store_bytes, 8);
    increment_pc(emu);
}

static void
execute_store_instruction(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = get_funct3(instruction);

    if (funct3 == 0) {
        sb(emu, instruction);
    }
    else if (funct3 == 1) {
        sh(emu, instruction);
    }
    else if (funct3 == 2) {
        sw(emu, instruction);
    }
    else if (funct3 == 3) {
        sd(emu, instruction);
    }
    else {
        ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
        abort();
    }
}

/* --------------------------- B-Type instructions ---------------------------*/

static void
beq(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs1 = get_rs1(instruction);
    const uint32_t rs2 = get_rs2(instruction);
    const uint32_t target = get_pc(emu) + b_type_get_immediate(instruction);

    if (rs1 == rs2) {
        set_pc(emu, target);
    }
    else {
        increment_pc(emu);
    }
}

static void
bne(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs1 = get_rs1(instruction);
    const uint32_t rs2 = get_rs2(instruction);
    const uint32_t target = get_pc(emu) + b_type_get_immediate(instruction);

    if (rs1 != rs2) {
        set_pc(emu, target);
    }
    else {
        increment_pc(emu);
    }
}

static void
blt(risc_v_emu_t* emu, const uint32_t instruction)
{
    const int32_t rs1 = (int32_t)get_rs1(instruction);
    const int32_t rs2 = (int32_t)get_rs2(instruction);
    const uint32_t target = get_pc(emu) + b_type_get_immediate(instruction);

    if (rs1 < rs2) {
        set_pc(emu, target);
    }
    else {
        increment_pc(emu);
    }
}

static void
bltu(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs1 = get_rs1(instruction);
    const uint32_t rs2 = get_rs2(instruction);
    const uint32_t target = get_pc(emu) + b_type_get_immediate(instruction);

    if (rs1 < rs2) {
        set_pc(emu, target);
    }
    else {
        increment_pc(emu);
    }
}

static void
bge(risc_v_emu_t* emu, const uint32_t instruction)
{
    const int32_t rs1 = (int32_t)get_rs1(instruction);
    const int32_t rs2 = (int32_t)get_rs2(instruction);
    const uint32_t target = get_pc(emu) + b_type_get_immediate(instruction);

    if (rs1 >= rs2) {
        set_pc(emu, target);
    }
    else {
        increment_pc(emu);
    }
}

static void
bgeu(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs1 = get_rs1(instruction);
    const uint32_t rs2 = get_rs2(instruction);
    const uint32_t target = get_pc(emu) + b_type_get_immediate(instruction);

    if (rs1 >= rs2) {
        set_pc(emu, target);
    }
    else {
        increment_pc(emu);
    }
}

static void
execute_branch_instruction(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = get_funct3(instruction);

    if (funct3 == 0) {
        beq(emu, instruction);
    }
    else if (funct3 == 1) {
        bne(emu, instruction);
    }
    else if (funct3 == 4) {
        blt(emu, instruction);
    }
    else if (funct3 == 5) {
        bge(emu, instruction);
    }
    else if (funct3 == 6) {
        bltu(emu, instruction);
    }
    else if (funct3 == 7) {
        bgeu(emu, instruction);
    }
    else {
        ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
        abort();
    }
}

/* ---------------------------- End instructions -----------------------------*/


/* ========================================================================== */
/*                            Emulator functions                              */
/* ========================================================================== */

// Execute the instruction which the pc is pointing to.
static void
risc_v_emu_execute_next_instruction(risc_v_emu_t* emu)
{
    // Emulate hard wired zero register.
    emu->registers[REG_ZERO] = 0;

    const uint32_t instruction = get_next_instruction(emu);
    const uint8_t  opcode      = get_opcode(instruction);

    ginger_log(INFO, "Current instruction: 0x%x\n", instruction);
    ginger_log(INFO, "Current opcode: 0x%x\n", opcode);

    // Execute the instruction.
    emu->instructions[opcode](emu, instruction);

    ++nb_executed_instructions;
    ginger_log(INFO, "Number of executed instructions: %lu\n", nb_executed_instructions);
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

    // API.
    emu->execute                                        = risc_v_emu_execute_next_instruction;
    emu->fork                                           = risc_v_emu_fork;
    emu->stack_push                                     = risc_v_emu_stack_push;
    emu->destroy                                        = risc_v_emu_destroy;

    // Functions corresponding to opcodes.
    emu->instructions[LUI]                              = lui;
    emu->instructions[AUIPC]                            = auipc;
    emu->instructions[JAL]                              = jal;
    emu->instructions[JALR]                             = jalr;
    emu->instructions[BRANCH]                           = execute_branch_instruction;
    emu->instructions[LOAD]                             = execute_load_instruction;
    emu->instructions[STORE]                            = execute_store_instruction;
    emu->instructions[ARITHMETIC_I_TYPE]                = execute_arithmetic_i_instruction;
    emu->instructions[ARITHMETIC_R_TYPE]                = execute_arithmetic_32_r_instruction;
    emu->instructions[FENCE]                            = fence;
    emu->instructions[ENV]                              = execute_env_instructions;
    emu->instructions[ARITHMETIC_64_REGISTER_IMMEDIATE] = execute_arithmetic_64_register_immediate_instructions;
    emu->instructions[ARITHMETIC_64_REGISTER_REGISTER]  = execute_arithmetic_64_register_register_instructions;

    return emu;
}
