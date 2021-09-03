#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "risc_v_emu.h"
#include "syscall.h"

#include "../shared/endianess_converter.h"
#include "../shared/logger.h"
#include "../shared/print_utils.h"
#include "../shared/vector.h"

// TODO: Remove globals.
uint64_t nb_executed_instructions = 0;

// Risc V 32i + 64i Instructions and corresponding opcode.
enum ENUM_OPCODE {
    OPCODE_FIRST,
    LUI                              = 0x37,
    AUIPC                            = 0x17,
    JAL                              = 0x6f,
    JALR                             = 0x67,
    BRANCH                           = 0x63,
    LOAD                             = 0x03,
    STORE                            = 0x23,
    ARITHMETIC_I_TYPE                = 0x13,
    ARITHMETIC_R_TYPE                = 0x33,
    FENCE                            = 0x0f,
    ENV                              = 0x73,
    ARITHMETIC_64_REGISTER_IMMEDIATE = 0x1b,
    ARITHMETIC_64_REGISTER_REGISTER  = 0x3b,
    OPCODE_LAST,
};

/* ========================================================================== */
/*                         Instruction meta functions                         */
/* ========================================================================== */

static uint32_t
get_funct3(const uint32_t instruction)
{
    return (instruction >> 12) & 0b111;
}

static uint32_t
get_funct7(const uint32_t instruction)
{
    return (instruction >> 25) & 0b1111111;
}

static char*
reg_to_str(const uint8_t reg)
{
    if (reg == 0) { return "ZERO\0"; }
    else if (reg == 1) { return "RA\0"; }
    else if (reg == 2) { return "SP\0"; }
    else if (reg == 3) { return "GP\0"; }
    else if (reg == 4) { return "TP\0"; }
    else if (reg == 5) { return "T0\0"; }
    else if (reg == 6) { return "T1\0"; }
    else if (reg == 7) { return "T2\0"; }
    else if (reg == 8) { return "FP\0"; }
    else if (reg == 9) { return "S1\0"; }
    else if (reg == 10) { return "A0\0"; }
    else if (reg == 11) { return "A1\0"; }
    else if (reg == 12) { return "A2\0"; }
    else if (reg == 13) { return "A3\0"; }
    else if (reg == 14) { return "A4\0"; }
    else if (reg == 15) { return "A5\0"; }
    else if (reg == 16) { return "A6\0"; }
    else if (reg == 17) { return "A7\0"; }
    else if (reg == 18) { return "S2\0"; }
    else if (reg == 19) { return "S3\0"; }
    else if (reg == 20) { return "S4\0"; }
    else if (reg == 21) { return "S5\0"; }
    else if (reg == 22) { return "S6\0"; }
    else if (reg == 23) { return "S7\0"; }
    else if (reg == 24) { return "S8\0"; }
    else if (reg == 25) { return "S9\0"; }
    else if (reg == 26) { return "S10\0"; }
    else if (reg == 27) { return "S11\0"; }
    else if (reg == 28) { return "T3\0"; }
    else if (reg == 29) { return "T4\0"; }
    else if (reg == 30) { return "T5\0"; }
    else if (reg == 31) { return "T6\0"; }
    else if (reg == 32) { return "PC\0"; }
    else { return '\0'; }
}

static int32_t
i_type_get_immediate(const uint32_t instruction)
{
    return (int32_t)instruction >> 20;
}

static int32_t
s_type_get_immediate(const uint32_t instruction)
{
    const uint32_t immediate40  = (instruction >> 7)  & 0b11111;
    const uint32_t immediate115 = (instruction >> 25) & 0b1111111;

    uint32_t target_immediate = (immediate115  << 5) | immediate40;
    target_immediate = ((int32_t)target_immediate << 20) >> 20;

    return (int32_t)target_immediate;
}

static int32_t
b_type_get_immediate(const uint32_t instruction)
{
    const uint32_t immediate11  = (instruction >> 7)  & 0b1;
    const uint32_t immediate41  = (instruction >> 8)  & 0b1111;
    const uint32_t immediate105 = (instruction >> 25) & 0b111111;
    const uint32_t immediate12  = (instruction >> 31) & 0b1;

    uint32_t target_immediate = (immediate12  << 12) |
                                (immediate11  << 11) |
                                (immediate105 << 5)  |
                                (immediate41  << 1);
    target_immediate = ((int32_t)target_immediate << 19) >> 19;

    return target_immediate;
}

static uint32_t
j_type_get_immediate(const uint32_t instruction)
{
    const uint32_t immediate20   = (instruction >> 31) & 0b1;
    const uint32_t immediate101  = (instruction >> 21) & 0b1111111111;
    const uint32_t immediate11   = (instruction >> 20) & 0b1;
    const uint32_t immediate1912 = (instruction >> 12) & 0b11111111;

    uint32_t target_immediate = (immediate20   << 20) |
                                (immediate1912 << 12) |
                                (immediate11   << 11) |
                                (immediate101  << 1);
    target_immediate = ((int32_t)target_immediate << 11) >> 11;

    return target_immediate;
}

uint64_t
get_reg(const risc_v_emu_t* emu, const uint8_t reg)
{
    return emu->registers[reg];
}

static uint64_t
get_rs1(const uint32_t instruction)
{
    return (instruction >> 15) & 0b11111;
}

static uint64_t
get_rs2(const uint32_t instruction)
{
    return (instruction >> 20) & 0b11111;
}

static uint64_t
get_reg_rs1(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint64_t rs1 = get_rs1(instruction);
    return get_reg(emu, rs1);
}

static uint64_t
get_reg_rs2(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs2 = get_rs2(instruction);
    return get_reg(emu, rs2);
}

static uint32_t
get_rd(const uint32_t instruction)
{
    return (instruction >> 7) & 0b11111;
}

static uint8_t
get_opcode(const uint32_t instruction)
{
    return instruction & 0b1111111;
}

static bool
validate_opcode(risc_v_emu_t* emu, const uint8_t opcode)
{
    if (emu->instructions[opcode] != 0) {
        return true;
    }
    else {
        return false;
    }
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
    return byte_arr_to_u64(instruction_bytes, 4, LSB);
}

void
set_reg(risc_v_emu_t* emu, const uint8_t reg, const uint64_t value)
{
    ginger_log(DEBUG, "Setting register %s to 0x%lx\n", reg_to_str(reg), value);
    emu->registers[reg] = value;
}

static uint64_t
get_pc(const risc_v_emu_t* emu)
{
    return get_reg(emu, REG_PC);
}

static void
set_pc(risc_v_emu_t* emu, const uint32_t value)
{
    set_reg(emu, REG_PC, value);
}

static void
increment_pc(risc_v_emu_t* emu)
{
    set_pc(emu, get_pc(emu) + 4);
}

static void
set_rd(risc_v_emu_t* emu, const uint32_t instruction, const uint64_t value)
{
    set_reg(emu, get_rd(instruction), value);
}


/* ========================================================================== */
/*                           Instruction functions                            */
/* ========================================================================== */

/* --------------------------- U-Type instructions ---------------------------*/

// Load upper immediate.
static void
lui(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          LUI\n");
    const uint64_t result = (uint64_t)(int32_t)(instruction & 0xfffff000);

    set_reg(emu, get_rd(instruction), result);
    increment_pc(emu);
}

// Add upper immediate to pc.
static void
auipc(risc_v_emu_t* emu, const uint32_t instruction)
{
    const int32_t addend  = (int32_t)(instruction & 0xfffff000);
    const int32_t result  = (int32_t)get_pc(emu) + addend;
    const uint8_t  ret_reg = get_rd(instruction);

    ginger_log(DEBUG, "Executing\t\tAUIPC\t%s,0x%x\n", reg_to_str(ret_reg), addend);

    set_reg(emu, ret_reg, result);
    increment_pc(emu);
}

/* --------------------------- J-Type instructions ---------------------------*/

static void
jal(risc_v_emu_t* emu, const uint32_t instruction)
{
    // When an unsigned int and an int are added together, the int is first
    // converted to unsigned int before the addition takes place. This makes
    // the following addition work.
    const int32_t jump_offset = j_type_get_immediate(instruction);
    const uint64_t result     = get_reg(emu, REG_PC) + jump_offset;

    ginger_log(DEBUG, "Executing\tJAL %s 0x%x\n", reg_to_str(get_rd(instruction)), result);

    const uint64_t return_address = get_reg(emu, REG_PC) + 4;
    set_reg(emu, get_rd(instruction), return_address);

    set_reg(emu, REG_PC, result);

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
    const int32_t  immediate    = i_type_get_immediate(instruction);
    const uint64_t register_rs1 = get_reg_rs1(emu, instruction);
    const int64_t  target       = (register_rs1 + immediate) & ~1;

    ginger_log(DEBUG, "Executing\tJALR %s\n", reg_to_str(get_rs1(instruction)));

    // Save pc + 4 into register rd.
    set_reg(emu, get_rd(instruction), get_reg(emu, REG_PC) + 4);

    // Jump to target address.
    set_pc(emu, target);
}

// Load byte.
static void
lb(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          LB\n");
    const uint64_t target   = get_reg_rs1(emu, instruction) + i_type_get_immediate(instruction);

    // Read 1 byte from target guest address into buffer.
    uint8_t loaded_bytes[1] = {0};
    emu->mmu->read(emu->mmu, loaded_bytes, target, 1);

    int32_t loaded_value = (int32_t)byte_arr_to_u64(loaded_bytes, 1, LSB);

    set_rd(emu, instruction, loaded_value);
    increment_pc(emu);
}

// Load half word.
static void
lh(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          LH\n");
    const uint64_t target   = get_reg_rs1(emu, instruction) + i_type_get_immediate(instruction);

    // Read 2 bytes from target guest address into buffer.
    uint8_t loaded_bytes[2] = {0};
    emu->mmu->read(emu->mmu, loaded_bytes, target, 2);

    // Sign-extend.
    int32_t loaded_value = (int32_t)(uint32_t)byte_arr_to_u64(loaded_bytes, 2, LSB);

    set_rd(emu, instruction, loaded_value);
    increment_pc(emu);
}

// Load word.
static void
lw(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint64_t target = get_reg_rs1(emu, instruction) + i_type_get_immediate(instruction);

    ginger_log(DEBUG, "Executing\tLW\n");
    ginger_log(DEBUG, "Loading 4 bytes from address: 0x%lx\n", target);

    // Read 4 bytes from target guest address into buffer.
    uint8_t loaded_bytes[4] = {0};
    emu->mmu->read(emu->mmu, loaded_bytes, target, 4);

    // Sign extend.
    int32_t loaded_value = (int32_t)(uint32_t)byte_arr_to_u64(loaded_bytes, 4, LSB);
    ginger_log(DEBUG, "Got value %d\n", loaded_value);

    set_rd(emu, instruction, loaded_value);
    increment_pc(emu);
}

// Load double word.
static void
ld(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint64_t base   = get_reg_rs1(emu, instruction);
    const uint32_t offset = i_type_get_immediate(instruction);
    const uint64_t target = base + offset;

    ginger_log(DEBUG, "Executing\t\tLD %s 0x%x\n",
               reg_to_str(get_rd(instruction)), target);

    uint8_t loaded_bytes[8] = {0};
    emu->mmu->read(emu->mmu, loaded_bytes, target, 8);
    const uint64_t result = byte_arr_to_u64(loaded_bytes, 8, LSB);

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

// Load byte unsigned.
static void
lbu(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          LBU\n");
    const uint64_t target = get_reg_rs1(emu, instruction) + i_type_get_immediate(instruction);

    // Read 1 byte from target guest address into buffer.
    uint8_t loaded_bytes[1] = {0};
    emu->mmu->read(emu->mmu, loaded_bytes, target, 1);

    uint32_t loaded_value = (uint32_t)byte_arr_to_u64(loaded_bytes, 1, LSB);

    set_rd(emu, instruction, loaded_value);
    increment_pc(emu);
}

// Load hald word unsigned.
static void
lhu(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          LHU\n");
    const uint64_t target = get_reg_rs1(emu, instruction) + i_type_get_immediate(instruction);

    // Read 2 bytes from target guest address into buffer.
    uint8_t loaded_bytes[2] = {0};
    emu->mmu->read(emu->mmu, loaded_bytes, target, 2);

    // Zero-extend to 32 bit.
    uint32_t loaded_value = (uint32_t)byte_arr_to_u64(loaded_bytes, 2, LSB);

    set_rd(emu, instruction, loaded_value);
    increment_pc(emu);
}

// Load word unsigned.
static void
lwu(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          LWU\n");
    const uint64_t base   = get_reg_rs1(emu, instruction);
    const uint32_t offset = i_type_get_immediate(instruction);
    const uint64_t target = base + offset;

    uint8_t loaded_bytes[4] = {0};
    emu->mmu->read(emu->mmu, loaded_bytes, target, 4);
    const uint64_t result = (uint64_t)byte_arr_to_u64(loaded_bytes, 4, LSB);

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
execute_load_instruction(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = get_funct3(instruction);
    ginger_log(DEBUG, "funct3 = %u\n", funct3);

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

// Add immediate. Also used to implement the pseudoinstructions mv and li. Adding a
// register with 0 and storing it in another register is the riscvi implementation of mv.
static void
addi(risc_v_emu_t* emu, const uint32_t instruction)
{
    const int32_t  addend = i_type_get_immediate(instruction);
    const uint64_t rs1    = get_reg_rs1(emu, instruction);
    const uint64_t result = (int64_t)(rs1 + addend); // Sign extend to 64 bit.

    ginger_log(DEBUG, "Executing\tADDI %s %s %d\n",
               reg_to_str(get_rd(instruction)),
               reg_to_str(rs1),
               addend);
    ginger_log(DEBUG, "Result: %ld\n", result);

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

// Set less than immediate.
static void
slti(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SLTI\n");
    const int32_t compare = i_type_get_immediate(instruction);

    if (get_reg_rs1(emu, instruction) < compare) {
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
    ginger_log(DEBUG, "Executing          SLTIU\n");
    const uint32_t immediate = (uint32_t)i_type_get_immediate(instruction);

    if (get_reg_rs1(emu, instruction) < immediate) {
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
    ginger_log(DEBUG, "Executing          XORI\n");
    const int32_t immediate = i_type_get_immediate(instruction);

    // Sign extend.
    const uint64_t result = (int64_t)(get_reg_rs1(emu, instruction) ^ immediate);
    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
ori(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          ORI\n");
    const uint32_t immediate = (uint32_t)i_type_get_immediate(instruction);
    const uint64_t result = get_reg_rs1(emu, instruction) | immediate;
    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
andi(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t immediate = (uint32_t)i_type_get_immediate(instruction);
    const uint64_t result    = get_reg_rs1(emu, instruction) & immediate;
    const uint8_t  ret_reg   = get_rd(instruction);

    ginger_log(DEBUG, "ANDI\t%s, %s, %u\n",
               reg_to_str(ret_reg),
               reg_to_str(get_rs1(instruction)),
               immediate);

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
slli(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t shamt  = i_type_get_immediate(instruction) & 0x3f;
    const uint64_t result = get_reg_rs1(emu, instruction) << shamt;
    const uint8_t  ret_reg = get_rd(instruction);

    ginger_log(DEBUG, "SLLI\t%s, %s, 0x%x\n",
               reg_to_str(ret_reg),
               reg_to_str(get_rs1(instruction)),
               shamt);

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
srli(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SRLI\n");
    const uint32_t shamt  = i_type_get_immediate(instruction) & 0x3f;
    const uint64_t result = get_reg_rs1(emu, instruction) >> shamt;
    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
srai(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SRAI\n");
    const uint32_t shamt  = i_type_get_immediate(instruction) & 0x3f;
    const uint64_t result = (uint64_t)((int64_t)get_reg_rs1(emu, instruction) >> shamt);
    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
execute_arithmetic_i_instruction(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = get_funct3(instruction);
    const uint32_t funct7 = get_funct7(instruction);
    ginger_log(DEBUG, "funct3 = %u\n", funct3);
    ginger_log(DEBUG, "funct7 = %u\n", funct7);

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

        // NOTE: According to the risc v specification, SRLI should equal funct7 == 0.
        //       This does not seem to be the case, according to our custom objdump.
        //       Somehow, srli can show up with funct7 set to 1. This should not happen.
        if (funct7 == 0 || funct7 == 1) {
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
    else {
        ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
        abort();
    }
}

static void
fence(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          FENCE\n");
    ginger_log(ERROR, "FENCE instruction not implemented!\n");
    abort();
}


// Syscall.
static void
ecall(risc_v_emu_t* emu)
{
    const uint64_t syscall_num = get_reg(emu, REG_A7);
    ginger_log(DEBUG, "Executing\tECALL %lu\n", syscall_num);
    handle_syscall(emu, syscall_num);
}

// The EBREAK instruction is used to return control to a debugging environment.
// We will not need this since we will not be "debugging" the target executables.
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
    ginger_log(DEBUG, "funct12 = %u\n", funct12);

    if (funct12 == 0) {
        ecall(emu);
    }
    else if (funct12 == 1) {
        ebreak(emu, instruction);
    }
    increment_pc(emu);
}

// Used to implement the sext.w (sign extend word) pseudo instruction.
static void
addiw(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          ADDIW\n");
    const int32_t immediate = i_type_get_immediate(instruction);
    const uint64_t rs1      = get_reg_rs1(emu, instruction);

    // TODO: Carefully monitor casting logic of following line.
    const uint64_t result = (int64_t)(rs1 + immediate);
    set_reg(emu, get_rd(instruction), result);
    increment_pc(emu);
}

static void
slliw(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SLLIW\n");
    const uint32_t shamt = i_type_get_immediate(instruction) & 0x1f;
    const uint64_t result = get_reg_rs1(emu, instruction) << shamt;
    set_reg(emu, get_rd(instruction), result);
    increment_pc(emu);
}

static void
srliw(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SRLIW\n");
    const uint32_t shamt = i_type_get_immediate(instruction) & 0x1f;
    const uint64_t result = get_reg_rs1(emu, instruction) >> shamt;
    set_reg(emu, get_rd(instruction), result);
    increment_pc(emu);
}

static void
sraiw(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SRAIW\n");
    const uint32_t shamt = i_type_get_immediate(instruction) & 0x1f;
    const uint64_t result = get_reg_rs1(emu, instruction) >> shamt;
    set_reg(emu, get_rd(instruction), (int32_t)result);
    increment_pc(emu);
}

static void
execute_arithmetic_64_register_immediate_instructions(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = get_funct3(instruction);
    const uint32_t funct7 = get_funct7(instruction);
    ginger_log(DEBUG, "funct3 = %u\n", funct3);
    ginger_log(DEBUG, "funct7 = %u\n", funct7);

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
    ginger_log(DEBUG, "Executing          ADDW\n");
    const uint64_t rs1  = get_reg_rs1(emu, instruction);
    const uint64_t rs2  = get_reg_rs2(emu, instruction);

    // Close your eyes!
    const uint64_t result = (uint64_t)(int64_t)(rs1 + rs2);

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
subw(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SUBW\n");
    const uint64_t rs1  = get_reg_rs1(emu, instruction);
    const uint64_t rs2  = get_reg_rs2(emu, instruction);

    const uint64_t result = (uint64_t)(int64_t)(rs1 - rs2);

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
sllw(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SLLW\n");
    const uint64_t rs1   = get_reg_rs1(emu, instruction);
    const uint64_t shamt = get_reg_rs2(emu, instruction) & 0b11111;

    const uint64_t result = (uint64_t)(int64_t)(rs1 << shamt);

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
srlw(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SRLW\n");
    const uint64_t rs1   = get_reg_rs1(emu, instruction);
    const uint64_t shamt = get_reg_rs2(emu, instruction) & 0x1f;

    const uint64_t result = (uint64_t)(int64_t)(rs1 >> shamt);

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
sraw(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SRAW\n");
    const uint64_t rs1   = get_reg_rs1(emu, instruction);
    const uint64_t src   = get_reg(emu, rs1);
    const uint64_t shamt = get_reg_rs2(emu, instruction) & 0x1f;

    const uint64_t result = (uint64_t)(int64_t)((int32_t)src >> shamt);

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
execute_arithmetic_64_register_register_instructions(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = get_funct3(instruction);
    const uint32_t funct7 = get_funct7(instruction);
    ginger_log(DEBUG, "funct3 = %u\n", funct3);
    ginger_log(DEBUG, "funct7 = %u\n", funct7);

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

/* --------------------------- R-Type instructions opcode: 0x33---------------------------*/

static void
add(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint64_t register_rs1 = get_reg_rs1(emu, instruction);
    const uint64_t register_rs2 = get_reg_rs2(emu, instruction);
    const uint64_t result       = register_rs1 + register_rs2;

    ginger_log(DEBUG, "ADD\t%s, %s, %s\n",
               reg_to_str(get_rd(instruction)),
               reg_to_str(get_rs1(instruction)),
               reg_to_str(get_rs2(instruction)));

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
sub(risc_v_emu_t* emu, const uint32_t instruction)
{
    const int64_t  register_rs1 = get_reg_rs1(emu, instruction);
    const int64_t  register_rs2 = get_reg_rs2(emu, instruction);
    const int64_t  result       = register_rs1 - register_rs2;
    const uint8_t  ret_reg      = get_rd(instruction);

    ginger_log(DEBUG, "Executing\tSUB\t%s, %s, %s\n",
               reg_to_str(ret_reg),
               reg_to_str(get_rs1(instruction)),
               reg_to_str(get_rs2(instruction)));

    ginger_log(DEBUG, "%ld - %ld  = %ld -> %s\n", register_rs1, register_rs2, result, reg_to_str(ret_reg));

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
sll(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint64_t register_rs1 = get_reg_rs1(emu, instruction);
    const uint64_t shift_value  = get_reg_rs2(emu, instruction) & 0b111111;
    const uint64_t result       = register_rs1 << shift_value;

    ginger_log(DEBUG, "Executing\tSLL\t%s, %s, %s\n", reg_to_str(get_rd(instruction)),
               reg_to_str(get_rs1(instruction)), reg_to_str(get_rs2(instruction)));

    ginger_log(DEBUG, "to shift: %lu, shift value: %lu, result: %lu\n", register_rs1, shift_value,
               result);

    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
slt(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SLT\n");
    const int64_t register_rs1 = (int64_t)get_reg_rs1(emu, instruction);
    const int64_t register_rs2 = (int64_t)get_reg_rs2(emu, instruction);

    if (register_rs1 < register_rs2) {
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
    ginger_log(DEBUG, "Executing          SLTU\n");
    const uint64_t register_rs1 = get_reg_rs1(emu, instruction);
    const uint64_t register_rs2 = get_reg_rs2(emu, instruction);

    if (register_rs1 < register_rs2) {
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
    ginger_log(DEBUG, "Executing          XOR\n");
    const uint64_t register_rs1 = get_reg_rs1(emu, instruction);
    const uint64_t register_rs2 = get_reg_rs2(emu, instruction);
    set_rd(emu, instruction, register_rs1 ^ register_rs2);
    increment_pc(emu);
}

static void
srl(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SRL\n");
    const uint64_t register_rs1 = get_reg_rs1(emu, instruction);
    const uint64_t shift_value  = get_reg_rs2(emu, instruction) & 0xf1;
    const uint64_t result       = register_rs1 >> shift_value;
    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
sra(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SRA\n");
    const uint64_t register_rs1 = get_reg_rs1(emu, instruction);
    const uint64_t shift_value  = get_reg_rs2(emu, instruction) & 0xf1;
    const uint64_t result       = (uint64_t)((int64_t)register_rs1 >> shift_value);
    set_rd(emu, instruction, result);
    increment_pc(emu);
}

static void
or(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          OR\n");
    const uint64_t register_rs1 = get_reg_rs1(emu, instruction);
    const uint64_t register_rs2 = get_reg_rs2(emu, instruction);
    set_rd(emu, instruction, register_rs1 | register_rs2);
    increment_pc(emu);
}

static void
and(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          AND\n");
    const uint64_t register_rs1 = get_reg_rs1(emu, instruction);
    const uint64_t register_rs2 = get_reg_rs2(emu, instruction);
    set_rd(emu, instruction, register_rs1 & register_rs2);
    increment_pc(emu);
}

static void
execute_arithmetic_r_instruction(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = get_funct3(instruction);
    const uint32_t funct7 = get_funct7(instruction);
    ginger_log(DEBUG, "funct3 = %u\n", funct3);
    ginger_log(DEBUG, "funct7 = %u\n", funct7);

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
    const uint64_t target      = get_reg_rs1(emu, instruction) + s_type_get_immediate(instruction);
    const uint8_t  store_value = get_reg_rs2(emu, instruction) & 0xff;

    ginger_log(DEBUG, "SB\t%s, %u(%s)\n",
               reg_to_str(get_rs1(instruction)),
               store_value,
               reg_to_str(get_rs2(instruction)));

    ginger_log(DEBUG, "Writing 0x%02x to 0x%x\n", store_value, target);

    emu->mmu->write(emu->mmu, target, &store_value, 1);
    increment_pc(emu);
}

static void
sh(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SH\n");
    const uint64_t target      = get_reg_rs1(emu, instruction) + s_type_get_immediate(instruction);
    const uint64_t store_value = get_reg_rs2(emu, instruction) & 0xffff;

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
    const uint64_t target      = get_reg_rs1(emu, instruction) + s_type_get_immediate(instruction);
    const uint64_t store_value = get_reg_rs2(emu, instruction) & 0xffffffff;

    uint8_t store_bytes[8] = {0};

    // TODO: Reuse variables above instead of running the functions again.
    ginger_log(DEBUG, "Executing\tSW %s, %d\n", reg_to_str(get_rs1(instruction)), s_type_get_immediate(instruction));
    ginger_log(DEBUG, "Target adr: 0x%x\n", target);
    ginger_log(DEBUG, "Storing value: 0x%lx\n", store_value);

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
    const uint64_t target      = get_reg_rs1(emu, instruction) + s_type_get_immediate(instruction);
    const uint64_t store_value = get_reg_rs2(emu, instruction) & 0xffffffffffffffff;

    uint8_t store_bytes[8] = {0};
    u64_to_byte_arr(store_value, store_bytes, LSB);

    ginger_log(DEBUG, "Executing\tSD 0x%x 0x%lx\n", target, store_value);

    // Write 8 bytes into guest memory at target address.
    emu->mmu->write(emu->mmu, target, store_bytes, 8);
    increment_pc(emu);
}

static void
execute_store_instruction(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = get_funct3(instruction);
    ginger_log(DEBUG, "funct3 = %u\n", funct3);

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
    ginger_log(DEBUG, "Executing          BEQ\n");
    const uint64_t register_rs1 = get_reg_rs1(emu, instruction);
    const uint64_t register_rs2 = get_reg_rs2(emu, instruction);
    const uint64_t target = get_pc(emu) + b_type_get_immediate(instruction);

    if (register_rs1 == register_rs2) {
        set_pc(emu, target);
    }
    else {
        increment_pc(emu);
    }
}

static void
bne(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint64_t register_rs1 = get_reg_rs1(emu, instruction);
    const uint64_t register_rs2 = get_reg_rs2(emu, instruction);
    const uint64_t target = get_pc(emu) + b_type_get_immediate(instruction);

    ginger_log(DEBUG, "BNE\t%s, %s, 0x%x\n",
               reg_to_str(get_rs1(instruction)),
               reg_to_str(get_rs2(instruction)),
               target);

    if (register_rs1 != register_rs2) {
        set_pc(emu, target);
        ginger_log(DEBUG, "Branch taken\n");
    }
    else {
        increment_pc(emu);
        ginger_log(DEBUG, "Branch not taken\n");
    }
}

static void
blt(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          BLT\n");
    const int64_t register_rs1 = (int64_t)get_reg_rs1(emu, instruction);
    const int64_t register_rs2 = (int64_t)get_reg_rs2(emu, instruction);
    const uint64_t target = get_pc(emu) + b_type_get_immediate(instruction);

    ginger_log(DEBUG, "BLT\t%s, %s, 0x%x\n",
               reg_to_str(get_rs1(instruction)),
               reg_to_str(get_rs2(instruction)),
               target);

    if (register_rs1 < register_rs2) {
        set_pc(emu, target);
    }
    else {
        increment_pc(emu);
    }
}

static void
bltu(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          BLTU\n");
    const uint64_t register_rs1 = get_reg_rs1(emu, instruction);
    const uint64_t register_rs2 = get_reg_rs2(emu, instruction);
    const uint64_t target = get_pc(emu) + b_type_get_immediate(instruction);

    ginger_log(DEBUG, "BLTU\t%s, %s, 0x%x\n",
               reg_to_str(get_rs1(instruction)),
               reg_to_str(get_rs2(instruction)),
               target);

    if (register_rs1 < register_rs2) {
        set_pc(emu, target);
    }
    else {
        increment_pc(emu);
    }
}

static void
bge(risc_v_emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          BGE\n");
    const int64_t register_rs1 = (int64_t)get_reg_rs1(emu, instruction);
    const int64_t register_rs2 = (int64_t)get_reg_rs2(emu, instruction);
    const uint64_t target = get_pc(emu) + b_type_get_immediate(instruction);

    ginger_log(DEBUG, "BGE\t%s, %s, 0x%x\n",
               reg_to_str(get_rs1(instruction)),
               reg_to_str(get_rs2(instruction)),
               target);

    if (register_rs1 >= register_rs2) {
        set_pc(emu, target);
    }
    else {
        increment_pc(emu);
    }
}

// Branch if register rs1 >= register rs2 unsigned.
static void
bgeu(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint64_t register_rs1 = get_reg_rs1(emu, instruction);
    const uint64_t register_rs2 = get_reg_rs2(emu, instruction);
    const uint64_t target = get_pc(emu) + b_type_get_immediate(instruction);

    ginger_log(DEBUG, "BGEU\t%s, %s, 0x%x\n",
               reg_to_str(get_rs1(instruction)),
               reg_to_str(get_rs2(instruction)),
               target);

    if (register_rs1 >= register_rs2) {
        set_pc(emu, target);
        ginger_log(DEBUG, "Branch taken\n");
    }
    else {
        increment_pc(emu);
        ginger_log(DEBUG, "Branch not taken\n");
    }
}

static void
execute_branch_instruction(risc_v_emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = get_funct3(instruction);
    ginger_log(DEBUG, "funct3 = %u\n", funct3);

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

    ginger_log(DEBUG, "=========================\n");
    ginger_log(DEBUG, "PC: 0x%x\n", get_pc(emu));
    ginger_log(DEBUG, "Number of executed instructions: %lu\n", nb_executed_instructions);
    ginger_log(DEBUG, "Instruction\t0x%08x\n", instruction);
    ginger_log(DEBUG, "Opcode\t\t0x%x\n", opcode);

    // Validate opcode - Can be removed for optimization purposes. Then we would
    // simply get a segfault istead of an error message, when an illegal opcode
    // is used.
    if (!validate_opcode(emu, opcode)) {
        ginger_log(ERROR, "Invalid opcode\t0x%x\n", opcode);
        abort();
    }

    // Execute the instruction.
    emu->instructions[opcode](emu, instruction);
    ++nb_executed_instructions;
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

    emu->stack_size = 1024 * 1024; // 1MiB stack.
    emu->mmu = mmu_create(memory_size, emu->stack_size);
    if (!emu->mmu) {
        ginger_log(ERROR, "[%s]Could not create mmu!\n", __func__);
        risc_v_emu_destroy(emu);
        return NULL;
    }

    // TODO: Might delete.
    emu->files = vector_create(sizeof(uint64_t));
    if (!emu->files) {
            ginger_log(ERROR, "[%s]Could not create file handle table!\n", __func__);
            risc_v_emu_destroy(emu);
            return NULL;
    }
    // Initial file descriptors.
    uint64_t _stdin  = 0;
    uint64_t _stdout = 1;
    uint64_t _stderr = 2;
    vector_append(emu->files, &_stdin);
    vector_append(emu->files, &_stdout);
    vector_append(emu->files, &_stderr);

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
    emu->instructions[ARITHMETIC_R_TYPE]                = execute_arithmetic_r_instruction;
    emu->instructions[FENCE]                            = fence;
    emu->instructions[ENV]                              = execute_env_instructions;
    emu->instructions[ARITHMETIC_64_REGISTER_IMMEDIATE] = execute_arithmetic_64_register_immediate_instructions;
    emu->instructions[ARITHMETIC_64_REGISTER_REGISTER]  = execute_arithmetic_64_register_register_instructions;

    emu->exit_reason = EMU_EXIT_REASON_NO_EXIT;

    return emu;
}
