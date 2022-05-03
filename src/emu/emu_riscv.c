#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <syscall.h>
#include <unistd.h>
#include <pthread.h>

#include "emu_riscv.h"
#include "syscall_riscv.h"

#include "../corpus/coverage.h"
#include "../corpus/corpus.h"
#include "../utils/endianess_converter.h"
#include "../utils/logger.h"
#include "../utils/vector.h"

// Risc V 32i + 64i Instructions and corresponding opcode.
enum ENUM_RISCV_OPCODE {
    ENUM_RISCV_LUI                              = 0x37,
    ENUM_RISCV_AUIPC                            = 0x17,
    ENUM_RISCV_JAL                              = 0x6f,
    ENUM_RISCV_JALR                             = 0x67,
    ENUM_RISCV_BRANCH                           = 0x63,
    ENUM_RISCV_LOAD                             = 0x03,
    ENUM_RISCV_STORE                            = 0x23,
    ENUM_RISCV_ARITHMETIC_I_TYPE                = 0x13,
    ENUM_RISCV_ARITHMETIC_R_TYPE                = 0x33,
    ENUM_RISCV_FENCE                            = 0x0f,
    ENUM_RISCV_ENV                              = 0x73,
    ENUM_RISCV_ARITHMETIC_64_REGISTER_IMMEDIATE = 0x1b,
    ENUM_RISCV_ARITHMETIC_64_REGISTER_REGISTER  = 0x3b,
};

// Function prototyp needed as both `emu_riscv_create` and `emu_riscv_fork` call
// eachother.
emu_t*
emu_riscv_create(size_t memory_size, corpus_t* corpus);

static void
emu_riscv_load_elf(emu_t* emu, const target_t* target)
{
    if (target->elf->length > emu->mmu->memory_size) {
        abort();
    }
    // Set the execution entry point.
    emu->set_pc(emu, target->elf->entry_point);

    // Load the loadable program headers into guest memory.
    for (int i = 0; i < target->elf->nb_prg_hdrs; i++) {
        const program_header_t* curr_prg_hdr = &target->elf->prg_hdrs[i];

        // Sanity check.
        if ((curr_prg_hdr->virtual_address + curr_prg_hdr->file_size) > (emu->mmu->memory_size - 1)) {
            ginger_log(ERROR, "[%s] Error! Write of 0x%lx bytes to address 0x%lx "
                    "would cause write outside of emulator memory!\n", __func__,
                    curr_prg_hdr->file_size, curr_prg_hdr->virtual_address);
            abort();
        }

        // We have to load the elf before memory is allocated in the emulator.
        // Thus we cannot check if a write would cause the emulator go out
        // of allocated memory. No memory should be allocated at this point.
        //
        // Set the permissions of the addresses where the loadable program
        // header will be loaded to writeable. We have to do this since the
        // memory we are about to write to is not yet allocated, and does not
        // have WRITE permissions set.
        emu->mmu->set_permissions(emu->mmu, curr_prg_hdr->virtual_address, MMU_PERM_WRITE, curr_prg_hdr->memory_size);

        // Load the executable segments of the binary into the emulator
        // NOTE: This write dirties the executable memory. Might want to make it
        //       clean before starting the emulator
        emu->mmu->write(emu->mmu, curr_prg_hdr->virtual_address, &target->elf->data[curr_prg_hdr->offset], curr_prg_hdr->file_size);

        // Fill padding with zeros.
        const int padding_len = curr_prg_hdr->memory_size - curr_prg_hdr->file_size;
        if (padding_len > 0) {
            uint8_t padding[padding_len];
            memset(padding, 0, padding_len);
            emu->mmu->write(emu->mmu, curr_prg_hdr->virtual_address + curr_prg_hdr->file_size, padding, padding_len);
        }

        // Set correct perms of loaded program header.
        emu->mmu->set_permissions(emu->mmu, curr_prg_hdr->virtual_address, curr_prg_hdr->flags, curr_prg_hdr->memory_size);

        // Updating the `curr_alloc_adr` here makes sure that the stack will never overwrite
        // the program headers, as long as it does not exceed `emu->stack_size`.
        const uint64_t program_hdr_end = ((curr_prg_hdr->virtual_address + curr_prg_hdr->memory_size) + 0xfff) & ~0xfff;
        if (program_hdr_end > emu->mmu->curr_alloc_adr) {
            emu->mmu->curr_alloc_adr = program_hdr_end;
        }

        // TODO: Make permissions print out part of ginger_log().
        ginger_log(INFO, "Wrote program header %lu of size 0x%lx to guest address 0x%lx with perms ", i,
                   curr_prg_hdr->file_size, curr_prg_hdr->virtual_address);
        print_permissions(curr_prg_hdr->flags);
        printf("\n");
    }
}

static void
emu_riscv_build_stack(emu_t* emu, const target_t* target)
{
    // Create a stack which starts at the curr_alloc_adr of the emulator.
    // Stack is 1MiB.
    uint8_t alloc_error = 0;
    const uint64_t stack_start = emu->mmu->allocate(emu->mmu, emu->stack_size, &alloc_error);
    if (alloc_error != 0) {
        ginger_log(ERROR, "Failed allocate memory for stack!\n");
    }

    // Stack grows downwards, so we set the stack pointer to starting address of the
    // stack + the stack size. As variables are allocated on the stack, their size
    // is subtracted from the stack pointer.
    emu->set_sp(emu, stack_start + emu->stack_size);

    ginger_log(INFO, "Stack start: 0x%lx\n", stack_start);
    ginger_log(INFO, "Stack size:  0x%lx\n", emu->stack_size);
    ginger_log(INFO, "Stack ptr:   0x%lx\n", emu->get_sp(emu));

    // Where the arguments got written to in guest memory is saved in this array.
    uint64_t guest_arg_addresses[target->argc];
    memset(&guest_arg_addresses, 0, sizeof(guest_arg_addresses));

    // Write all provided arguments into guest memory.
    for (int i = 0; i < target->argc; i++) {
        // Populate program name memory segment.
        const uint64_t arg_adr = emu->mmu->allocate(emu->mmu, ARG_MAX, &alloc_error);
        if (alloc_error != 0) {
            ginger_log(ERROR, "Failed allocate memory for target program argument!\n");
        }
        guest_arg_addresses[i] = arg_adr;
        emu->mmu->write(emu->mmu, arg_adr, (uint8_t*)target->argv[i].string, target->argv[i].length);

        // Make arg segment read and writeable.
        emu->mmu->set_permissions(emu->mmu, arg_adr, MMU_PERM_READ | MMU_PERM_WRITE, ARG_MAX);

        ginger_log(INFO, "arg[%d] \"%s\" written to guest adr: 0x%lx\n", i, target->argv[i].string, arg_adr);
    }

    ginger_log(INFO, "Building initial stack at guest address: 0x%x\n", emu->get_sp(emu));

    // Push the dummy values filled with zero onto the stack as 64 bit values.
    uint8_t auxp[8]     = {0};
    uint8_t envp[8]     = {0};
    uint8_t argv_end[8] = {0};
    emu->stack_push(emu, auxp, 8);
    emu->stack_push(emu, envp, 8);
    emu->stack_push(emu, argv_end, 8);

    // Push the guest addresses of the program arguments onto the stack.
    for (int i = target->argc - 1; i >= 0; i--) {
        uint8_t arg_buf[8] = {0};
        u64_to_byte_arr(guest_arg_addresses[i], arg_buf, LSB);
        emu->stack_push(emu, arg_buf, 8); // Push the argument.
    }

    // Push argc onto the stack.
    uint8_t argc_buf[8] = {0};
    u64_to_byte_arr(target->argc, argc_buf, LSB);
    emu->stack_push(emu, argc_buf, 8);
}

/* ========================================================================== */
/*                              Print functions                               */
/* ========================================================================== */

static void
emu_riscv_print_registers(emu_t* emu)
{
    printf("\nzero\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_ZERO]);
    printf("ra\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_RA]);
    printf("sp\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_SP]);
    printf("gp\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_GP]);
    printf("tp\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_TP]);
    printf("t0\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_T0]);
    printf("t1\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_T1]);
    printf("t2\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_T2]);
    printf("fp\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_FP]);
    printf("s1\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_S1]);
    printf("a0\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_A0]);
    printf("a1\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_A1]);
    printf("a2\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_A2]);
    printf("a3\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_A3]);
    printf("a4\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_A4]);
    printf("a5\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_A5]);
    printf("a6\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_A6]);
    printf("a7\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_A7]);
    printf("s2\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_S2]);
    printf("s3\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_S3]);
    printf("s4\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_S4]);
    printf("s5\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_S5]);
    printf("s6\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_S6]);
    printf("s7\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_S7]);
    printf("s8\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_S8]);
    printf("s9\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_S9]);
    printf("s10\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_S10]);
    printf("s11\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_S11]);
    printf("t3\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_T3]);
    printf("t4\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_T4]);
    printf("t5\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_T5]);
    printf("t6\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_T6]);
    printf("pc\t0x%"PRIx64"\n", emu->registers[RISC_V_REG_PC]);
}

/* ========================================================================== */
/*                         Instruction meta functions                         */
/* ========================================================================== */

static uint32_t
emu_riscv_get_funct3(const uint32_t instruction)
{
    return (instruction >> 12) & 0b111;
}

static uint32_t
emu_riscv_get_funct7(const uint32_t instruction)
{
    return (instruction >> 25) & 0b1111111;
}

static char*
emu_riscv_reg_to_str(const uint8_t reg)
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
emu_riscv_i_type_get_immediate(const uint32_t instruction)
{
    return (int32_t)instruction >> 20;
}

static int32_t
emu_riscv_s_type_get_immediate(const uint32_t instruction)
{
    const uint32_t immediate40  = (instruction >> 7)  & 0b11111;
    const uint32_t immediate115 = (instruction >> 25) & 0b1111111;

    uint32_t target_immediate = (immediate115  << 5) | immediate40;
    target_immediate = ((int32_t)target_immediate << 20) >> 20;

    return (int32_t)target_immediate;
}

static int32_t
emu_riscv_b_type_get_immediate(const uint32_t instruction)
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
emu_riscv_j_type_get_immediate(const uint32_t instruction)
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

static uint64_t
emu_riscv_get_reg(const emu_t* emu, const uint8_t reg)
{
    return emu->registers[reg];
}

static uint64_t
emu_riscv_get_rs1(const uint32_t instruction)
{
    return (instruction >> 15) & 0b11111;
}

static uint64_t
emu_riscv_get_rs2(const uint32_t instruction)
{
    return (instruction >> 20) & 0b11111;
}

static uint64_t
emu_riscv_get_reg_rs1(emu_t* emu, const uint32_t instruction)
{
    const uint64_t rs1 = emu_riscv_get_rs1(instruction);
    return emu_riscv_get_reg(emu, rs1);
}

static uint64_t
emu_riscv_get_reg_rs2(emu_t* emu, const uint32_t instruction)
{
    const uint32_t rs2 = emu_riscv_get_rs2(instruction);
    return emu_riscv_get_reg(emu, rs2);
}

static uint32_t
emu_riscv_get_rd(const uint32_t instruction)
{
    return (instruction >> 7) & 0b11111;
}

static uint8_t
emu_riscv_get_opcode(const uint32_t instruction)
{
    return instruction & 0b1111111;
}

static bool
emu_riscv_validate_opcode(emu_t* emu, const uint8_t opcode)
{
    if (emu->instructions[opcode] != 0) {
        return true;
    }
    else {
        return false;
    }
}

static uint32_t
emu_riscv_get_next_instruction(const emu_t* emu)
{
    uint8_t instruction_bytes[4] = {0};
    for (int i = 0; i < 4; i++) {
        const uint8_t current_permission = emu->mmu->permissions[emu->registers[RISC_V_REG_PC] + i];
        if ((current_permission & MMU_PERM_EXEC) == 0) {
            ginger_log(ERROR, "No exec perm set on address: 0x%x\n", emu->registers[RISC_V_REG_PC] + i);
            abort();
        }
        instruction_bytes[i] = emu->mmu->memory[emu->registers[RISC_V_REG_PC] + i];
    }
    return byte_arr_to_u64(instruction_bytes, 4, LSB);
}

void
emu_riscv_set_reg(emu_t* emu, const uint8_t reg, const uint64_t value)
{
    ginger_log(DEBUG, "Setting register %s to 0x%lx\n", emu_riscv_reg_to_str(reg), value);
    emu->registers[reg] = value;
}

static uint64_t
emu_riscv_get_pc(const emu_t* emu)
{
    return emu_riscv_get_reg(emu, RISC_V_REG_PC);
}

static void
emu_riscv_set_pc(emu_t* emu, const uint64_t value)
{
    emu_riscv_set_reg(emu, RISC_V_REG_PC, value);
}

static void
emu_riscv_increment_pc(emu_t* emu)
{
    emu_riscv_set_pc(emu, emu_riscv_get_pc(emu) + 4);
}

static void
emu_riscv_set_rd(emu_t* emu, const uint32_t instruction, const uint64_t value)
{
    emu_riscv_set_reg(emu, emu_riscv_get_rd(instruction), value);
}

static uint64_t
emu_riscv_get_sp(const emu_t* emu)
{
    return emu_riscv_get_reg(emu, RISC_V_REG_SP);
}

static void
emu_riscv_set_sp(emu_t* emu, const uint64_t value)
{
    emu_riscv_set_reg(emu, RISC_V_REG_SP, value);
}


/* ========================================================================== */
/*                           Instruction functions                            */
/* ========================================================================== */

/* --------------------------- U-Type instructions ---------------------------*/

// Load upper immediate.
static void
emu_riscv_lui(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          LUI\n");
    const uint64_t result = (uint64_t)(int32_t)(instruction & 0xfffff000);

    emu_riscv_set_reg(emu, emu_riscv_get_rd(instruction), result);
    emu_riscv_increment_pc(emu);
}

// Add upper immediate to pc.
static void
emu_riscv_auipc(emu_t* emu, const uint32_t instruction)
{
    const int32_t addend  = (int32_t)(instruction & 0xfffff000);
    const int32_t result  = (int32_t)emu_riscv_get_pc(emu) + addend;
    const uint8_t  ret_reg = emu_riscv_get_rd(instruction);

    ginger_log(DEBUG, "Executing\t\tAUIPC\t%s,0x%x\n", emu_riscv_reg_to_str(ret_reg), addend);

    emu_riscv_set_reg(emu, ret_reg, result);
    emu_riscv_increment_pc(emu);
}

/* --------------------------- J-Type instructions ---------------------------*/

static void
emu_riscv_jal(emu_t* emu, const uint32_t instruction)
{
    // When an unsigned int and an int are added together, the int is first
    // converted to unsigned int before the addition takes place. This makes
    // the following addition work.
    const int32_t  jump_offset = emu_riscv_j_type_get_immediate(instruction);
    const uint64_t pc          = emu_riscv_get_pc(emu);
    const uint64_t ret         = pc + 4;
    const uint64_t target      = pc + jump_offset;

    ginger_log(DEBUG, "Executing\tJAL %s 0x%x\n", emu_riscv_reg_to_str(emu_riscv_get_rd(instruction)), target);
    emu_riscv_set_reg(emu, emu_riscv_get_rd(instruction), ret);
    emu->new_coverage = coverage_on_branch(emu->corpus->coverage, pc, target);
    emu_riscv_set_reg(emu, RISC_V_REG_PC, target);

    // TODO: Make use of following if statement.
    //
    // Jump is unconditional jump.
    if (emu_riscv_get_rd(instruction) == RISC_V_REG_ZERO) {
        return;
    }
    // Jump is function call.
    else {
        return;
    }
}

/* --------------------------- I-Type instructions ---------------------------*/

static void
emu_riscv_jalr(emu_t* emu, const uint32_t instruction)
{
    // Calculate target jump address.
    const int32_t  immediate    = emu_riscv_i_type_get_immediate(instruction);
    const uint64_t register_rs1 = emu_riscv_get_reg_rs1(emu, instruction);
    const int64_t  target       = (register_rs1 + immediate) & ~1;
    const uint64_t pc           = emu_riscv_get_pc(emu);
    const uint64_t ret          = pc + 4;

    ginger_log(DEBUG, "Executing\tJALR %s\n", emu_riscv_reg_to_str(emu_riscv_get_rs1(instruction)));

    // Save ret into register rd.
    emu_riscv_set_reg(emu, emu_riscv_get_rd(instruction), ret);

    emu->new_coverage = coverage_on_branch(emu->corpus->coverage, pc, target);

    // Jump to target address.
    emu_riscv_set_pc(emu, target);
}

// Load byte.
static void
emu_riscv_lb(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          LB\n");
    const uint64_t target   = emu_riscv_get_reg_rs1(emu, instruction) + emu_riscv_i_type_get_immediate(instruction);

    // Read 1 byte from target guest address into buffer.
    uint8_t       loaded_bytes[1] = {0};
    const uint8_t read_ok         = emu->mmu->read(emu->mmu, loaded_bytes, target, 1);
    if (read_ok != 0) {
        emu->exit_reason = EMU_EXIT_REASON_SEGFAULT_READ;
        return;
    }

    int32_t loaded_value = (int32_t)byte_arr_to_u64(loaded_bytes, 1, LSB);

    emu_riscv_set_rd(emu, instruction, loaded_value);
    emu_riscv_increment_pc(emu);
}

// Load half word.
static void
emu_riscv_lh(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          LH\n");
    const uint64_t target   = emu_riscv_get_reg_rs1(emu, instruction) + emu_riscv_i_type_get_immediate(instruction);

    // Read 2 bytes from target guest address into buffer.
    uint8_t       loaded_bytes[2] = {0};
    const uint8_t read_ok         = emu->mmu->read(emu->mmu, loaded_bytes, target, 2);
    if (read_ok != 0) {
        emu->exit_reason = EMU_EXIT_REASON_SEGFAULT_READ;
        return;
    }

    // Sign-extend.
    int32_t loaded_value = (int32_t)(uint32_t)byte_arr_to_u64(loaded_bytes, 2, LSB);

    emu_riscv_set_rd(emu, instruction, loaded_value);
    emu_riscv_increment_pc(emu);
}

// Load word.
static void
emu_riscv_lw(emu_t* emu, const uint32_t instruction)
{
    const uint64_t target = emu_riscv_get_reg_rs1(emu, instruction) + emu_riscv_i_type_get_immediate(instruction);

    ginger_log(DEBUG, "Executing\tLW\n");
    ginger_log(DEBUG, "Loading 4 bytes from address: 0x%lx\n", target);

    // Read 4 bytes from target guest address into buffer.
    uint8_t       loaded_bytes[4] = {0};
    const uint8_t read_ok         = emu->mmu->read(emu->mmu, loaded_bytes, target, 4);
    if (read_ok != 0) {
        emu->exit_reason = EMU_EXIT_REASON_SEGFAULT_READ;
        return;
    }

    // Sign extend.
    int32_t loaded_value = (int32_t)(uint32_t)byte_arr_to_u64(loaded_bytes, 4, LSB);
    ginger_log(DEBUG, "Got value %d\n", loaded_value);

    emu_riscv_set_rd(emu, instruction, loaded_value);
    emu_riscv_increment_pc(emu);
}

// Load double word.
static void
emu_riscv_ld(emu_t* emu, const uint32_t instruction)
{
    const uint64_t target = emu_riscv_get_reg_rs1(emu, instruction) + emu_riscv_i_type_get_immediate(instruction);

    ginger_log(DEBUG, "Executing\t\tLD %s 0x%lx\n", emu_riscv_reg_to_str(emu_riscv_get_rd(instruction)), target);

    uint8_t       loaded_bytes[8] = {0};
    const uint8_t read_ok         = emu->mmu->read(emu->mmu, loaded_bytes, target, 8);
    if (read_ok != 0) {
        emu->exit_reason = EMU_EXIT_REASON_SEGFAULT_READ;
        return;
    }

    const uint64_t result = byte_arr_to_u64(loaded_bytes, 8, LSB);

    emu_riscv_set_rd(emu, instruction, result);
    emu_riscv_increment_pc(emu);
}

// Load byte unsigned.
static void
emu_riscv_lbu(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          LBU\n");
    const uint64_t target = emu_riscv_get_reg_rs1(emu, instruction) + emu_riscv_i_type_get_immediate(instruction);

    // Read 1 byte from target guest address into buffer.
    uint8_t       loaded_bytes[1] = {0};
    const uint8_t read_ok         = emu->mmu->read(emu->mmu, loaded_bytes, target, 1);
    if (read_ok != 0) {
        emu->exit_reason = EMU_EXIT_REASON_SEGFAULT_READ;
        return;
    }

    uint32_t loaded_value = (uint32_t)byte_arr_to_u64(loaded_bytes, 1, LSB);

    emu_riscv_set_rd(emu, instruction, loaded_value);
    emu_riscv_increment_pc(emu);
}

// Load hald word unsigned.
static void
emu_riscv_lhu(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          LHU\n");
    const uint64_t target = emu_riscv_get_reg_rs1(emu, instruction) + emu_riscv_i_type_get_immediate(instruction);

    // Read 2 bytes from target guest address into buffer.
    uint8_t       loaded_bytes[2] = {0};
    const uint8_t read_ok         = emu->mmu->read(emu->mmu, loaded_bytes, target, 2);
    if (read_ok != 0) {
        emu->exit_reason = EMU_EXIT_REASON_SEGFAULT_READ;
        return;
    }

    // Zero-extend to 32 bit.
    uint32_t loaded_value = (uint32_t)byte_arr_to_u64(loaded_bytes, 2, LSB);

    emu_riscv_set_rd(emu, instruction, loaded_value);
    emu_riscv_increment_pc(emu);
}

// Load word unsigned.
static void
emu_riscv_lwu(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          LWU\n");
    const uint64_t base   = emu_riscv_get_reg_rs1(emu, instruction);
    const uint32_t offset = emu_riscv_i_type_get_immediate(instruction);
    const uint64_t target = base + offset;

    uint8_t       loaded_bytes[4] = {0};
    const uint8_t read_ok         = emu->mmu->read(emu->mmu, loaded_bytes, target, 4);
    if (read_ok != 0) {
        emu->exit_reason = EMU_EXIT_REASON_SEGFAULT_READ;
        return;
    }

    const uint64_t result = (uint64_t)byte_arr_to_u64(loaded_bytes, 4, LSB);

    emu_riscv_set_rd(emu, instruction, result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_execute_load_instruction(emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = emu_riscv_get_funct3(instruction);
    ginger_log(DEBUG, "funct3 = %u\n", funct3);

    if (funct3 == 0) {
        emu_riscv_lb(emu, instruction);
    }
    else if (funct3 == 1) {
        emu_riscv_lh(emu, instruction);
    }
    else if (funct3 == 2) {
        emu_riscv_lw(emu, instruction);
    }
    else if (funct3 == 3) {
        emu_riscv_ld(emu, instruction);
    }
    else if (funct3 == 4) {
        emu_riscv_lbu(emu, instruction);
    }
    else if (funct3 == 5) {
        emu_riscv_lhu(emu, instruction);
    }
    else if (funct3 == 6) {
        emu_riscv_lwu(emu, instruction);
    }
    else {
        ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
        abort();
    }
}

// Add immediate. Also used to implement the pseudoinstructions mv and li. Adding a
// register with 0 and storing it in another register is the riscvi implementation of mv.
static void
emu_riscv_addi(emu_t* emu, const uint32_t instruction)
{
    const int32_t  addend = emu_riscv_i_type_get_immediate(instruction);
    const uint64_t rs1    = emu_riscv_get_reg_rs1(emu, instruction);
    const uint64_t result = (int64_t)(rs1 + addend); // Sign extend to 64 bit.

    ginger_log(DEBUG, "Executing\tADDI %s %s %d\n",
               emu_riscv_reg_to_str(emu_riscv_get_rd(instruction)),
               emu_riscv_reg_to_str(rs1),
               addend);
    ginger_log(DEBUG, "Result: %ld\n", result);

    emu_riscv_set_rd(emu, instruction, result);
    emu_riscv_increment_pc(emu);
}

// Set less than immediate.
static void
emu_riscv_slti(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SLTI\n");
    const int32_t compare = emu_riscv_i_type_get_immediate(instruction);

    if (emu_riscv_get_reg_rs1(emu, instruction) < compare) {
        emu_riscv_set_rd(emu, instruction, 1);
    }
    else {
        emu_riscv_set_rd(emu, instruction, 0);
    }
    emu_riscv_increment_pc(emu);
}

// Set less than immediate.
static void
emu_riscv_sltiu(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SLTIU\n");
    const uint32_t immediate = (uint32_t)emu_riscv_i_type_get_immediate(instruction);

    if (emu_riscv_get_reg_rs1(emu, instruction) < immediate) {
        emu_riscv_set_rd(emu, instruction, 1);
    }
    else {
        emu_riscv_set_rd(emu, instruction, 0);
    }
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_xori(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          XORI\n");
    const int32_t immediate = emu_riscv_i_type_get_immediate(instruction);

    // Sign extend.
    const uint64_t result = (int64_t)(emu_riscv_get_reg_rs1(emu, instruction) ^ immediate);
    emu_riscv_set_rd(emu, instruction, result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_ori(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          ORI\n");
    const uint32_t immediate = (uint32_t)emu_riscv_i_type_get_immediate(instruction);
    const uint64_t result = emu_riscv_get_reg_rs1(emu, instruction) | immediate;
    emu_riscv_set_rd(emu, instruction, result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_andi(emu_t* emu, const uint32_t instruction)
{
    const uint32_t immediate = emu_riscv_i_type_get_immediate(instruction);
    const uint64_t result    = emu_riscv_get_reg_rs1(emu, instruction) & immediate;
    const uint8_t  ret_reg   = emu_riscv_get_rd(instruction);

    ginger_log(DEBUG, "ANDI\t%s, %s, %u\n",
               emu_riscv_reg_to_str(ret_reg),
               emu_riscv_reg_to_str(emu_riscv_get_rs1(instruction)),
               immediate);

    emu_riscv_set_rd(emu, instruction, result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_slli(emu_t* emu, const uint32_t instruction)
{
    const uint32_t shamt  = emu_riscv_i_type_get_immediate(instruction) & 0x3f;
    const uint64_t result = emu_riscv_get_reg_rs1(emu, instruction) << shamt;
    const uint8_t  ret_reg = emu_riscv_get_rd(instruction);

    ginger_log(DEBUG, "SLLI\t%s, %s, 0x%x\n",
               emu_riscv_reg_to_str(ret_reg),
               emu_riscv_reg_to_str(emu_riscv_get_rs1(instruction)),
               shamt);

    emu_riscv_set_rd(emu, instruction, result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_srli(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SRLI\n");
    const uint32_t shamt  = emu_riscv_i_type_get_immediate(instruction) & 0x3f;
    const uint64_t result = emu_riscv_get_reg_rs1(emu, instruction) >> shamt;
    emu_riscv_set_rd(emu, instruction, result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_srai(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SRAI\n");
    const uint32_t shamt  = emu_riscv_i_type_get_immediate(instruction) & 0x3f;
    const uint64_t result = (uint64_t)((int64_t)emu_riscv_get_reg_rs1(emu, instruction) >> shamt);
    emu_riscv_set_rd(emu, instruction, result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_execute_arithmetic_i_instruction(emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = emu_riscv_get_funct3(instruction);
    const uint32_t funct7 = emu_riscv_get_funct7(instruction);
    ginger_log(DEBUG, "funct3 = %u\n", funct3);
    ginger_log(DEBUG, "funct7 = %u\n", funct7);

    if (funct3 == 0) {
        emu_riscv_addi(emu, instruction);
    }
    else if (funct3 == 1) {
        emu_riscv_slli(emu, instruction);
    }
    else if (funct3 == 2) {
        emu_riscv_slti(emu, instruction);
    }
    else if (funct3 == 3) {
        emu_riscv_sltiu(emu, instruction);
    }
    else if (funct3 == 4) {
        emu_riscv_xori(emu, instruction);
    }
    else if (funct3 == 5) {

        // NOTE: According to the risc v specification, SRLI should equal funct7 == 0.
        //       This does not seem to be the case, according to our custom objdump.
        //       Somehow, srli can show up with funct7 set to 1. This should not happen.
        if (funct7 == 0 || funct7 == 1) {
            emu_riscv_srli(emu, instruction);
        }
        // NOTE: This does not follow the specification! SRAI should only be executed
        //       when funct7 is 16, according to the risc v 2019-12-13 specification.
        else if (funct7 == 16 || funct7 == 32 || funct7 == 33) {
            emu_riscv_srai(emu, instruction);
        }
        else {
            ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
            abort();
        }
    }
    else if (funct3 == 6) {
        emu_riscv_ori(emu, instruction);
    }
    else if (funct3 == 7) {
        emu_riscv_andi(emu, instruction);
    }
    else if (funct3 == 1) {
        emu_riscv_slli(emu, instruction);
    }
    else {
        ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
        abort();
    }
}

static void
emu_riscv_fence(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          FENCE\n");
    ginger_log(ERROR, "FENCE instruction not implemented!\n");
    abort();
}

// Syscall.
static void
emu_riscv_ecall(emu_t* emu)
{
    const uint64_t syscall_num = emu_riscv_get_reg(emu, RISC_V_REG_A7);
    ginger_log(DEBUG, "Executing\tECALL %lu\n", syscall_num);
    handle_syscall(emu, syscall_num);
}

// The EBREAK instruction is used to return control to a debugging environment.
// We will not need this since we will not be "debugging" the target executables.
static void
emu_riscv_ebreak(emu_t* emu, const uint32_t instruction)
{
    ginger_log(ERROR, "EBREAK instruction not implemented!\n");
    abort();
}

static void
emu_riscv_execute_env_instructions(emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct12 = emu_riscv_i_type_get_immediate(instruction);
    ginger_log(DEBUG, "funct12 = %u\n", funct12);

    if (funct12 == 0) {
        emu_riscv_ecall(emu);
    }
    else if (funct12 == 1) {
        emu_riscv_ebreak(emu, instruction);
    }
    emu_riscv_increment_pc(emu);
}

// Used to implement the sext.w (sign extend word) pseudo instruction.
static void
emu_riscv_addiw(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          ADDIW\n");
    const int32_t immediate = emu_riscv_i_type_get_immediate(instruction);
    const uint64_t rs1      = emu_riscv_get_reg_rs1(emu, instruction);

    // TODO: Carefully monitor casting logic of following line.
    const uint64_t result = (int64_t)(rs1 + immediate);
    emu_riscv_set_reg(emu, emu_riscv_get_rd(instruction), result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_slliw(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SLLIW\n");
    const uint32_t shamt = emu_riscv_i_type_get_immediate(instruction) & 0x1f;
    const uint64_t result = emu_riscv_get_reg_rs1(emu, instruction) << shamt;
    emu_riscv_set_reg(emu, emu_riscv_get_rd(instruction), result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_srliw(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SRLIW\n");
    const uint32_t shamt = emu_riscv_i_type_get_immediate(instruction) & 0x1f;
    const uint64_t result = emu_riscv_get_reg_rs1(emu, instruction) >> shamt;
    emu_riscv_set_reg(emu, emu_riscv_get_rd(instruction), result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_sraiw(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SRAIW\n");
    const uint32_t shamt = emu_riscv_i_type_get_immediate(instruction) & 0x1f;
    const uint64_t result = emu_riscv_get_reg_rs1(emu, instruction) >> shamt;
    emu_riscv_set_reg(emu, emu_riscv_get_rd(instruction), (int32_t)result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_execute_arithmetic_64_register_immediate_instructions(emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = emu_riscv_get_funct3(instruction);
    const uint32_t funct7 = emu_riscv_get_funct7(instruction);
    ginger_log(DEBUG, "funct3 = %u\n", funct3);
    ginger_log(DEBUG, "funct7 = %u\n", funct7);

    if (funct3 == 0) {
        emu_riscv_addiw(emu, instruction);
    }
    else if (funct3 == 1) {
        emu_riscv_slliw(emu, instruction);
    }
    else if (funct3 == 5) {
        if (funct7 == 0 ) {
            emu_riscv_srliw(emu, instruction);
        }
        else if (funct7 == 32 ) {
            emu_riscv_sraiw(emu, instruction);
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
emu_riscv_addw(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          ADDW\n");
    const uint64_t rs1  = emu_riscv_get_reg_rs1(emu, instruction);
    const uint64_t rs2  = emu_riscv_get_reg_rs2(emu, instruction);

    // Close your eyes!
    const uint64_t result = (uint64_t)(int64_t)(rs1 + rs2);

    emu_riscv_set_rd(emu, instruction, result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_subw(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SUBW\n");
    const uint64_t rs1  = emu_riscv_get_reg_rs1(emu, instruction);
    const uint64_t rs2  = emu_riscv_get_reg_rs2(emu, instruction);

    const uint64_t result = (uint64_t)(int64_t)(rs1 - rs2);

    emu_riscv_set_rd(emu, instruction, result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_sllw(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SLLW\n");
    const uint64_t rs1   = emu_riscv_get_reg_rs1(emu, instruction);
    const uint64_t shamt = emu_riscv_get_reg_rs2(emu, instruction) & 0b11111;

    const uint64_t result = (uint64_t)(int64_t)(rs1 << shamt);

    emu_riscv_set_rd(emu, instruction, result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_srlw(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SRLW\n");
    const uint64_t rs1   = emu_riscv_get_reg_rs1(emu, instruction);
    const uint64_t shamt = emu_riscv_get_reg_rs2(emu, instruction) & 0x1f;

    const uint64_t result = (uint64_t)(int64_t)(rs1 >> shamt);

    emu_riscv_set_rd(emu, instruction, result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_sraw(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SRAW\n");
    const uint64_t rs1   = emu_riscv_get_reg_rs1(emu, instruction);
    const uint64_t src   = emu_riscv_get_reg(emu, rs1);
    const uint64_t shamt = emu_riscv_get_reg_rs2(emu, instruction) & 0x1f;

    const uint64_t result = (uint64_t)(int64_t)((int32_t)src >> shamt);

    emu_riscv_set_rd(emu, instruction, result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_execute_arithmetic_64_register_register_instructions(emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = emu_riscv_get_funct3(instruction);
    const uint32_t funct7 = emu_riscv_get_funct7(instruction);
    ginger_log(DEBUG, "funct3 = %u\n", funct3);
    ginger_log(DEBUG, "funct7 = %u\n", funct7);

    if (funct3 == 0) {
        if (funct7 == 0) {
            emu_riscv_addw(emu, instruction);
        }
        else if (funct7 == 32) {
            emu_riscv_subw(emu, instruction);
        }
        else {
            ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
            abort();
        }
    }
    else if (funct3 == 1) {
        emu_riscv_sllw(emu, instruction);
    }
    else if (funct3 == 5) {
        if (funct7 == 0) {
            emu_riscv_srlw(emu, instruction);
        }
        else if (funct7 == 32) {
            emu_riscv_sraw(emu, instruction);
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
emu_riscv_add(emu_t* emu, const uint32_t instruction)
{
    const uint64_t register_rs1 = emu_riscv_get_reg_rs1(emu, instruction);
    const uint64_t register_rs2 = emu_riscv_get_reg_rs2(emu, instruction);
    const uint64_t result       = register_rs1 + register_rs2;

    ginger_log(DEBUG, "ADD\t%s, %s, %s\n",
               emu_riscv_reg_to_str(emu_riscv_get_rd(instruction)),
               emu_riscv_reg_to_str(emu_riscv_get_rs1(instruction)),
               emu_riscv_reg_to_str(emu_riscv_get_rs2(instruction)));

    emu_riscv_set_rd(emu, instruction, result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_sub(emu_t* emu, const uint32_t instruction)
{
    const int64_t  register_rs1 = emu_riscv_get_reg_rs1(emu, instruction);
    const int64_t  register_rs2 = emu_riscv_get_reg_rs2(emu, instruction);
    const int64_t  result       = register_rs1 - register_rs2;
    const uint8_t  ret_reg      = emu_riscv_get_rd(instruction);

    ginger_log(DEBUG, "Executing\tSUB\t%s, %s, %s\n",
               emu_riscv_reg_to_str(ret_reg),
               emu_riscv_reg_to_str(emu_riscv_get_rs1(instruction)),
               emu_riscv_reg_to_str(emu_riscv_get_rs2(instruction)));

    ginger_log(DEBUG, "%ld - %ld  = %ld -> %s\n", register_rs1, register_rs2, result, emu_riscv_reg_to_str(ret_reg));

    emu_riscv_set_rd(emu, instruction, result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_sll(emu_t* emu, const uint32_t instruction)
{
    const uint64_t register_rs1 = emu_riscv_get_reg_rs1(emu, instruction);
    const uint64_t shift_value  = emu_riscv_get_reg_rs2(emu, instruction) & 0b111111;
    const uint64_t result       = register_rs1 << shift_value;

    ginger_log(DEBUG, "Executing\tSLL\t%s, %s, %s\n", emu_riscv_reg_to_str(emu_riscv_get_rd(instruction)),
               emu_riscv_reg_to_str(emu_riscv_get_rs1(instruction)), emu_riscv_reg_to_str(emu_riscv_get_rs2(instruction)));

    ginger_log(DEBUG, "to shift: %lu, shift value: %lu, result: %lu\n", register_rs1, shift_value,
               result);

    emu_riscv_set_rd(emu, instruction, result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_slt(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SLT\n");
    const int64_t register_rs1 = (int64_t)emu_riscv_get_reg_rs1(emu, instruction);
    const int64_t register_rs2 = (int64_t)emu_riscv_get_reg_rs2(emu, instruction);

    if (register_rs1 < register_rs2) {
        emu_riscv_set_rd(emu, instruction, 1);
    }
    else {
        emu_riscv_set_rd(emu, instruction, 0);
    }
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_sltu(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SLTU\n");
    const uint64_t register_rs1 = emu_riscv_get_reg_rs1(emu, instruction);
    const uint64_t register_rs2 = emu_riscv_get_reg_rs2(emu, instruction);

    if (register_rs1 < register_rs2) {
        emu_riscv_set_rd(emu, instruction, 1);
    }
    else {
        emu_riscv_set_rd(emu, instruction, 0);
    }
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_xor(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          XOR\n");
    const uint64_t register_rs1 = emu_riscv_get_reg_rs1(emu, instruction);
    const uint64_t register_rs2 = emu_riscv_get_reg_rs2(emu, instruction);
    emu_riscv_set_rd(emu, instruction, register_rs1 ^ register_rs2);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_srl(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SRL\n");
    const uint64_t register_rs1 = emu_riscv_get_reg_rs1(emu, instruction);
    const uint64_t shift_value  = emu_riscv_get_reg_rs2(emu, instruction) & 0xf1;
    const uint64_t result       = register_rs1 >> shift_value;
    emu_riscv_set_rd(emu, instruction, result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_sra(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SRA\n");
    const uint64_t register_rs1 = emu_riscv_get_reg_rs1(emu, instruction);
    const uint64_t shift_value  = emu_riscv_get_reg_rs2(emu, instruction) & 0xf1;
    const uint64_t result       = (uint64_t)((int64_t)register_rs1 >> shift_value);
    emu_riscv_set_rd(emu, instruction, result);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_or(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          OR\n");
    const uint64_t register_rs1 = emu_riscv_get_reg_rs1(emu, instruction);
    const uint64_t register_rs2 = emu_riscv_get_reg_rs2(emu, instruction);
    emu_riscv_set_rd(emu, instruction, register_rs1 | register_rs2);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_and(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          AND\n");
    const uint64_t register_rs1 = emu_riscv_get_reg_rs1(emu, instruction);
    const uint64_t register_rs2 = emu_riscv_get_reg_rs2(emu, instruction);
    emu_riscv_set_rd(emu, instruction, register_rs1 & register_rs2);
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_execute_arithmetic_r_instruction(emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = emu_riscv_get_funct3(instruction);
    const uint32_t funct7 = emu_riscv_get_funct7(instruction);
    ginger_log(DEBUG, "funct3 = %u\n", funct3);
    ginger_log(DEBUG, "funct7 = %u\n", funct7);

    if (funct3 == 0) {
        if (funct7 == 0) {
            emu_riscv_add(emu, instruction);
        }
        else if (funct7 == 32) {
            emu_riscv_sub(emu, instruction);
        }
        else {
            ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
            abort();
        }
    }
    else if (funct3 == 1) {
        emu_riscv_sll(emu, instruction);
    }
    else if (funct3 == 2) {
        emu_riscv_slt(emu, instruction);
    }
    else if (funct3 == 3) {
        emu_riscv_sltu(emu, instruction);
    }
    else if (funct3 == 4) {
        emu_riscv_xor(emu, instruction);
    }
    else if (funct3 == 5) {
        if (funct7 == 0) {
            emu_riscv_srl(emu, instruction);
        }
        else if (funct7 == 32) {
            emu_riscv_sra(emu, instruction);
        }
        else {
            ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
            abort();
        }
    }
    else if (funct3 == 6) {
        emu_riscv_or(emu, instruction);
    }
    else if (funct3 == 7) {
        emu_riscv_and(emu, instruction);
    }
    else {
        ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
        abort();
    }
}

/* --------------------------- S-Type instructions ---------------------------*/

static void
emu_riscv_sb(emu_t* emu, const uint32_t instruction)
{
    const uint64_t target      = emu_riscv_get_reg_rs1(emu, instruction) + emu_riscv_s_type_get_immediate(instruction);
    const uint8_t  store_value = emu_riscv_get_reg_rs2(emu, instruction) & 0xff;

    ginger_log(DEBUG, "SB\t%s, %u(%s)\n",
               emu_riscv_reg_to_str(emu_riscv_get_rs1(instruction)),
               store_value,
               emu_riscv_reg_to_str(emu_riscv_get_rs2(instruction)));

    ginger_log(DEBUG, "Writing 0x%02x to 0x%x\n", store_value, target);

    const uint8_t write_ok = emu->mmu->write(emu->mmu, target, &store_value, 1);
    if (write_ok != 0) {
        emu->exit_reason = EMU_EXIT_REASON_SEGFAULT_WRITE;
        return;
    }
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_sh(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "Executing          SH\n");
    const uint64_t target      = emu_riscv_get_reg_rs1(emu, instruction) + emu_riscv_s_type_get_immediate(instruction);
    const uint64_t store_value = emu_riscv_get_reg_rs2(emu, instruction) & 0xffff;

    // TODO: Update u64_to_byte_arr to be able to handle smaller integers,
    //       removing the need for 8 byte array here.
    uint8_t store_bytes[8] = {0};
    u64_to_byte_arr(store_value, store_bytes, LSB);

    // Write 2 bytes into guest memory at target address.
    const uint8_t write_ok = emu->mmu->write(emu->mmu, target, store_bytes, 2);
    if (write_ok != 0) {
        emu->exit_reason = EMU_EXIT_REASON_SEGFAULT_WRITE;
        return;
    }
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_sw(emu_t* emu, const uint32_t instruction)
{
    const uint64_t target      = emu_riscv_get_reg_rs1(emu, instruction) + emu_riscv_s_type_get_immediate(instruction);
    const uint64_t store_value = emu_riscv_get_reg_rs2(emu, instruction) & 0xffffffff;

    uint8_t store_bytes[8] = {0};

    // TODO: Reuse variables above instead of running the functions again.
    ginger_log(DEBUG, "Executing\tSW %s, %d\n", emu_riscv_reg_to_str(emu_riscv_get_rs1(instruction)), emu_riscv_s_type_get_immediate(instruction));
    ginger_log(DEBUG, "Target adr: 0x%x\n", target);
    ginger_log(DEBUG, "Storing value: 0x%lx\n", store_value);

    // TODO: Update u64_to_byte_arr to be able to handle smaller integers,
    //       removing the need for 8 byte array here.
    u64_to_byte_arr(store_value, store_bytes, LSB);

    // Write 4 bytes into guest memory at target address.
    const uint8_t write_ok = emu->mmu->write(emu->mmu, target, store_bytes, 4);
    if (write_ok != 0) {
        emu->exit_reason = EMU_EXIT_REASON_SEGFAULT_WRITE;
        return;
    }
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_sd(emu_t* emu, const uint32_t instruction)
{
    const uint64_t target      = emu_riscv_get_reg_rs1(emu, instruction) + emu_riscv_s_type_get_immediate(instruction);
    const uint64_t store_value = emu_riscv_get_reg_rs2(emu, instruction) & 0xffffffffffffffff;

    uint8_t store_bytes[8] = {0};
    u64_to_byte_arr(store_value, store_bytes, LSB);

    ginger_log(DEBUG, "Executing\tSD 0x%x 0x%lx\n", target, store_value);

    // Write 8 bytes into guest memory at target address.
    const uint8_t write_ok = emu->mmu->write(emu->mmu, target, store_bytes, 8);
    if (write_ok != 0) {
        emu->exit_reason = EMU_EXIT_REASON_SEGFAULT_WRITE;
        return;
    }
    emu_riscv_increment_pc(emu);
}

static void
emu_riscv_execute_store_instruction(emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = emu_riscv_get_funct3(instruction);
    ginger_log(DEBUG, "funct3 = %u\n", funct3);

    if (funct3 == 0) {
        emu_riscv_sb(emu, instruction);
    }
    else if (funct3 == 1) {
        emu_riscv_sh(emu, instruction);
    }
    else if (funct3 == 2) {
        emu_riscv_sw(emu, instruction);
    }
    else if (funct3 == 3) {
        emu_riscv_sd(emu, instruction);
    }
    else {
        ginger_log(ERROR, "[%s:%u] Invalid instruction!\n", __func__, __LINE__);
        abort();
    }
}

/* --------------------------- B-Type instructions ---------------------------*/

static void
emu_riscv_beq(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "BEQ\n");
    const uint64_t register_rs1 = emu_riscv_get_reg_rs1(emu, instruction);
    const uint64_t register_rs2 = emu_riscv_get_reg_rs2(emu, instruction);
    const uint64_t pc           = emu_riscv_get_pc(emu);
    const uint64_t target       = pc + emu_riscv_b_type_get_immediate(instruction);

    if (register_rs1 == register_rs2) {
        emu->new_coverage = coverage_on_branch(emu->corpus->coverage, pc, target);
        emu_riscv_set_pc(emu, target);
    }
    else {
        emu_riscv_increment_pc(emu);
    }
}

static void
emu_riscv_bne(emu_t* emu, const uint32_t instruction)
{
    const uint64_t register_rs1 = emu_riscv_get_reg_rs1(emu, instruction);
    const uint64_t register_rs2 = emu_riscv_get_reg_rs2(emu, instruction);
    const uint64_t pc           = emu_riscv_get_pc(emu);
    const uint64_t target       = pc + emu_riscv_b_type_get_immediate(instruction);

    ginger_log(DEBUG, "BNE\t%s, %s, 0x%x\n",
               emu_riscv_reg_to_str(emu_riscv_get_rs1(instruction)),
               emu_riscv_reg_to_str(emu_riscv_get_rs2(instruction)),
               target);

    if (register_rs1 != register_rs2) {
        emu->new_coverage = coverage_on_branch(emu->corpus->coverage, pc, target);
        emu_riscv_set_pc(emu, target);
    }
    else {
        emu_riscv_increment_pc(emu);
    }
}

static void
emu_riscv_blt(emu_t* emu, const uint32_t instruction)
{
    ginger_log(DEBUG, "BLT\n");
    const int64_t  register_rs1 = emu_riscv_get_reg_rs1(emu, instruction);
    const int64_t  register_rs2 = emu_riscv_get_reg_rs2(emu, instruction);
    const uint64_t pc           = emu_riscv_get_pc(emu);
    const uint64_t target       = pc + emu_riscv_b_type_get_immediate(instruction);

    ginger_log(DEBUG, "BLT\t%s, %s, 0x%x\n",
               emu_riscv_reg_to_str(emu_riscv_get_rs1(instruction)),
               emu_riscv_reg_to_str(emu_riscv_get_rs2(instruction)),
               target);

    if (register_rs1 < register_rs2) {
        emu->new_coverage = coverage_on_branch(emu->corpus->coverage, pc, target);
        emu_riscv_set_pc(emu, target);
    }
    else {
        emu_riscv_increment_pc(emu);
    }
}

static void
emu_riscv_bltu(emu_t* emu, const uint32_t instruction)
{
    const uint64_t register_rs1 = emu_riscv_get_reg_rs1(emu, instruction);
    const uint64_t register_rs2 = emu_riscv_get_reg_rs2(emu, instruction);
    const uint64_t pc           = emu_riscv_get_pc(emu);
    const uint64_t target       = pc + emu_riscv_b_type_get_immediate(instruction);

    ginger_log(DEBUG, "BLTU\t%s, %s, 0x%x\n",
               emu_riscv_reg_to_str(emu_riscv_get_rs1(instruction)),
               emu_riscv_reg_to_str(emu_riscv_get_rs2(instruction)),
               target);

    if (register_rs1 < register_rs2) {
        emu->new_coverage = coverage_on_branch(emu->corpus->coverage, pc, target);
        emu_riscv_set_pc(emu, target);
    }
    else {
        emu_riscv_increment_pc(emu);
    }
}

static void
emu_riscv_bge(emu_t* emu, const uint32_t instruction)
{
    const int64_t  register_rs1 = emu_riscv_get_reg_rs1(emu, instruction);
    const int64_t  register_rs2 = emu_riscv_get_reg_rs2(emu, instruction);
    const uint64_t pc           = emu_riscv_get_pc(emu);
    const uint64_t target       = pc + emu_riscv_b_type_get_immediate(instruction);

    ginger_log(DEBUG, "BGE\t%s, %s, 0x%x\n",
               emu_riscv_reg_to_str(emu_riscv_get_rs1(instruction)),
               emu_riscv_reg_to_str(emu_riscv_get_rs2(instruction)),
               target);

    if (register_rs1 >= register_rs2) {
        emu->new_coverage = coverage_on_branch(emu->corpus->coverage, pc, target);
        emu_riscv_set_pc(emu, target);
    }
    else {
        emu_riscv_increment_pc(emu);
    }
}

// Branch if register rs1 >= register rs2 unsigned.
static void
emu_riscv_bgeu(emu_t* emu, const uint32_t instruction)
{
    const uint64_t register_rs1 = emu_riscv_get_reg_rs1(emu, instruction);
    const uint64_t register_rs2 = emu_riscv_get_reg_rs2(emu, instruction);
    const uint64_t pc           = emu_riscv_get_pc(emu);
    const uint64_t target       = pc + emu_riscv_b_type_get_immediate(instruction);

    ginger_log(DEBUG, "BGEU\t%s, %s, 0x%x\n",
               emu_riscv_reg_to_str(emu_riscv_get_rs1(instruction)),
               emu_riscv_reg_to_str(emu_riscv_get_rs2(instruction)),
               target);

    if (register_rs1 >= register_rs2) {
        emu->new_coverage = coverage_on_branch(emu->corpus->coverage, pc, target);
        emu_riscv_set_pc(emu, target);
    }
    else {
        emu_riscv_increment_pc(emu);
    }
}

static void
emu_riscv_execute_branch_instruction(emu_t* emu, const uint32_t instruction)
{
    const uint32_t funct3 = emu_riscv_get_funct3(instruction);
    ginger_log(DEBUG, "funct3 = %u\n", funct3);

    if (funct3 == 0) {
        emu_riscv_beq(emu, instruction);
    }
    else if (funct3 == 1) {
        emu_riscv_bne(emu, instruction);
    }
    else if (funct3 == 4) {
        emu_riscv_blt(emu, instruction);
    }
    else if (funct3 == 5) {
        emu_riscv_bge(emu, instruction);
    }
    else if (funct3 == 6) {
        emu_riscv_bltu(emu, instruction);
    }
    else if (funct3 == 7) {
        emu_riscv_bgeu(emu, instruction);
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
emu_riscv_execute_next_instruction(emu_t* emu)
{
    // Emulate hard wired zero register.
    emu->registers[RISC_V_REG_ZERO] = 0;

    const uint32_t instruction = emu_riscv_get_next_instruction(emu);
    const uint8_t  opcode      = emu_riscv_get_opcode(instruction);

    ginger_log(DEBUG, "=========================\n");
    ginger_log(DEBUG, "PC: 0x%x\n", emu_riscv_get_pc(emu));
    ginger_log(DEBUG, "Instruction\t0x%08x\n", instruction);
    ginger_log(DEBUG, "Opcode\t\t0x%x\n", opcode);

    // Validate opcode - Can be removed for optimization purposes. Then we would
    // simply get a segfault istead of an error message, when an illegal opcode
    // is used.
    if (!emu_riscv_validate_opcode(emu, opcode)) {
        ginger_log(ERROR, "Invalid opcode\t0x%x\n", opcode);
        emu->exit_reason = EMU_EXIT_REASON_INVALID_OPCODE;
        return;
    }

    // Execute the instruction.
    emu->instructions[opcode](emu, instruction);
}

// Reset the dirty blocks of an emulator to that of another emulator. This function needs to be
// really fast, since resetting emulators is the main action of the fuzzer.
static void
emu_riscv_reset(emu_t* dst_emu, const emu_t* src_emu)
{
    // Reset the dirty blocks in memory.
    for (uint64_t i = 0; i < dst_emu->mmu->dirty_state->nb_dirty_blocks; i++) {

        const uint64_t block = dst_emu->mmu->dirty_state->dirty_blocks[i];

        // Starting address of the dirty block in guest memory.
        const uint64_t block_adr = block * DIRTY_BLOCK_SIZE;

        // Copy the memory and perms corresponding to the dirty block from the source emu
        // to the destination emu.
        memcpy(dst_emu->mmu->memory +      block_adr, src_emu->mmu->memory +      block_adr, DIRTY_BLOCK_SIZE);
        memcpy(dst_emu->mmu->permissions + block_adr, src_emu->mmu->permissions + block_adr, DIRTY_BLOCK_SIZE);

        // Reset the allocation pointer.
        dst_emu->mmu->curr_alloc_adr = src_emu->mmu->curr_alloc_adr;

        // Clear the bitmap entry corresponding to the dirty block.
        // We could calculate the bit index here and `logicaly and` it to zero, but we
        // will still have to do a 64 bit write, so might as well skip the bit index
        // calculation.
        dst_emu->mmu->dirty_state->dirty_bitmap[block / 64] = 0;
    }
    dst_emu->mmu->dirty_state->clear(dst_emu->mmu->dirty_state);

    // Reset register state.
    // TODO: This memcpy almost triples the reset time. Optimize.
    memcpy(dst_emu->registers, src_emu->registers, sizeof(dst_emu->registers));

    dst_emu->exit_reason = EMU_EXIT_REASON_NO_EXIT;
    dst_emu->new_coverage = false;
}

static void
emu_riscv_report_exit_reason(emu_stats_t* stats, enum_emu_exit_reasons_t exit_reason)
{
    switch(exit_reason) {
    case EMU_EXIT_REASON_SYSCALL_NOT_SUPPORTED:
        emu_stats_inc(stats, EMU_COUNTERS_EXIT_REASON_SYSCALL_NOT_SUPPORTED);
        break;
    case EMU_EXIT_REASON_FSTAT_BAD_FD:
        emu_stats_inc(stats, EMU_COUNTERS_EXIT_FSTAT_BAD_FD);
        break;
    case EMU_EXIT_REASON_SEGFAULT_READ:
        emu_stats_inc(stats, EMU_COUNTERS_EXIT_SEGFAULT_READ);
        break;
    case EMU_EXIT_REASON_SEGFAULT_WRITE:
        emu_stats_inc(stats, EMU_COUNTERS_EXIT_SEGFAULT_WRITE);
        break;
    case EMU_EXIT_REASON_INVALID_OPCODE:
        emu_stats_inc(stats, EMU_COUNTERS_EXIT_INVALID_OPCODE);
        break;
    case EMU_EXIT_REASON_GRACEFUL:
        emu_stats_inc(stats, EMU_COUNTERS_EXIT_GRACEFUL);
        break;
    case EMU_EXIT_REASON_NO_EXIT:
        break;
    }
}

// Run an emulator until it exits or crashes.
static enum_emu_exit_reasons_t
emu_riscv_run(emu_t* emu, emu_stats_t* stats)
{
    // Execute the next instruction as long as no exit reason is set.
    while (emu->exit_reason == EMU_EXIT_REASON_NO_EXIT) {
        emu->execute(emu);
        emu_stats_inc(stats, EMU_COUNTERS_EXECUTED_INSTRUCTIONS);
    }
    // Report why emulator exited.
    emu_riscv_report_exit_reason(stats, emu->exit_reason);
    return emu->exit_reason;
}

static enum_emu_exit_reasons_t
emu_riscv_run_until(emu_t* emu, emu_stats_t* stats, const uint64_t break_adr)
{
    while (emu->exit_reason == EMU_EXIT_REASON_NO_EXIT && emu_riscv_get_reg(emu, RISC_V_REG_PC) != break_adr) {
        emu->execute(emu);
        emu_stats_inc(stats, EMU_COUNTERS_EXECUTED_INSTRUCTIONS);
    }
    // If we exited, crashed or encountered unknown behavior, report it.
    if (emu->exit_reason == EMU_EXIT_REASON_NO_EXIT) {
        emu_riscv_report_exit_reason(stats, emu->exit_reason);
    }
    return emu->exit_reason;
}

static void
emu_riscv_stack_push(emu_t* emu, uint8_t bytes[], size_t nb_bytes)
{
    const uint8_t write_ok = emu->mmu->write(emu->mmu, emu->registers[RISC_V_REG_SP] - nb_bytes, bytes, nb_bytes);
    if (write_ok != 0) {
        emu->exit_reason = EMU_EXIT_REASON_SEGFAULT_WRITE;
        return;
    }
    emu->registers[RISC_V_REG_SP] -= nb_bytes;
}

// Return a pointer to a new, forked emulator.
static emu_t*
emu_riscv_fork(const emu_t* emu)
{
    emu_t* forked = emu_riscv_create(emu->mmu->memory_size, emu->corpus);
    if (!forked) {
        ginger_log(ERROR, "[%s] Failed to fork emu!\n", __func__);
        abort();
    }

    // Copy register state.
    memcpy(&forked->registers, &emu->registers, sizeof(forked->registers));

    // Copy memory and permission state.
    memcpy(forked->mmu->memory, emu->mmu->memory, forked->mmu->memory_size);
    memcpy(forked->mmu->permissions, emu->mmu->permissions, forked->mmu->memory_size);

    // Set the current allocation address.
    forked->mmu->curr_alloc_adr = emu->mmu->curr_alloc_adr;

    return forked;
}

// Free all the internal data of the risc v emulator. The `emu_t` struct
// itself is freed by calling `emu_generic_destroy()`.
void
emu_riscv_destroy(emu_t* emu)
{
    if (emu) {
        if (emu->mmu) {
            mmu_destroy(emu->mmu);
        }
        free(emu);
    }
}

emu_t*
emu_riscv_create(size_t memory_size, corpus_t* corpus)
{
    emu_t* emu = calloc(1, sizeof(emu_t));
    if (!emu) {
        ginger_log(ERROR, "[%s]Could not create emu!\n", __func__);
        return NULL;
    }

    emu->stack_size = 1024 * 1024; // 1MiB stack.
    emu->mmu = mmu_create(memory_size, emu->stack_size);
    if (!emu->mmu) {
        ginger_log(ERROR, "[%s]Could not create mmu!\n", __func__);
        abort();
    }

    // API.
    emu->load_elf        = emu_riscv_load_elf;
    emu->build_stack     = emu_riscv_build_stack;
    emu->execute         = emu_riscv_execute_next_instruction;
    emu->fork            = emu_riscv_fork;
    emu->reset           = emu_riscv_reset;
    emu->run             = emu_riscv_run;
    emu->run_until       = emu_riscv_run_until;
    emu->stack_push      = emu_riscv_stack_push;
    emu->print_regs      = emu_riscv_print_registers;
    emu->get_reg         = emu_riscv_get_reg;
    emu->set_reg         = emu_riscv_set_reg;
    emu->get_pc          = emu_riscv_get_pc;
    emu->set_pc          = emu_riscv_set_pc;
    emu->get_sp          = emu_riscv_get_sp;
    emu->set_sp          = emu_riscv_set_sp;

    // Functions corresponding to opcodes.
    emu->instructions[ENUM_RISCV_LUI]                              = emu_riscv_lui;
    emu->instructions[ENUM_RISCV_AUIPC]                            = emu_riscv_auipc;
    emu->instructions[ENUM_RISCV_JAL]                              = emu_riscv_jal;
    emu->instructions[ENUM_RISCV_JALR]                             = emu_riscv_jalr;
    emu->instructions[ENUM_RISCV_BRANCH]                           = emu_riscv_execute_branch_instruction;
    emu->instructions[ENUM_RISCV_LOAD]                             = emu_riscv_execute_load_instruction;
    emu->instructions[ENUM_RISCV_STORE]                            = emu_riscv_execute_store_instruction;
    emu->instructions[ENUM_RISCV_ARITHMETIC_I_TYPE]                = emu_riscv_execute_arithmetic_i_instruction;
    emu->instructions[ENUM_RISCV_ARITHMETIC_R_TYPE]                = emu_riscv_execute_arithmetic_r_instruction;
    emu->instructions[ENUM_RISCV_FENCE]                            = emu_riscv_fence;
    emu->instructions[ENUM_RISCV_ENV]                              = emu_riscv_execute_env_instructions;
    emu->instructions[ENUM_RISCV_ARITHMETIC_64_REGISTER_IMMEDIATE] = emu_riscv_execute_arithmetic_64_register_immediate_instructions;
    emu->instructions[ENUM_RISCV_ARITHMETIC_64_REGISTER_REGISTER]  = emu_riscv_execute_arithmetic_64_register_register_instructions;

    emu->exit_reason  = EMU_EXIT_REASON_NO_EXIT;
    emu->new_coverage = false;
    emu->corpus       = corpus;

    return emu;
}
