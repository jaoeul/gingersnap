#include <stdio.h>
#include <string.h>

#include "mips64msb.h"

#include "../../mmu/adr_map.h"
#include "../../utils/endianess.h"
#include "../../utils/logger.h"
#include "../../utils/print_utils.h"

typedef enum {
    MIPS64MSB_INST_SPECIAL                = 0b000000,
    MIPS64MSB_INST_REGIMM                 = 0b000001,
    MIPS64MSB_INST_J                      = 0b000010,
    MIPS64MSB_INST_JAL                    = 0b000011,
    MIPS64MSB_INST_BEQ                    = 0b000100,
    MIPS64MSB_INST_BNE                    = 0b000101,
    MIPS64MSB_INST_BLEZ_POP06             = 0b000110,
    MIPS64MSB_INST_BGTZ                   = 0b000111,

    MIPS64MSB_INST_ADDI                   = 0b001000,
    MIPS64MSB_INST_ADDIU                  = 0b001001,
    MIPS64MSB_INST_SLTI                   = 0b001010,
    MIPS64MSB_INST_SLTIU                  = 0b001011,
    MIPS64MSB_INST_ANDI                   = 0b001100,
    MIPS64MSB_INST_ORI                    = 0b001101,
    MIPS64MSB_INST_XORI                   = 0b001110,
    MIPS64MSB_INST_LUI                    = 0b001111,

    MIPS64MSB_INST_COP0                   = 0b010000,
    MIPS64MSB_INST_COP1                   = 0b010001,
    MIPS64MSB_INST_COP2                   = 0b010010,
    MIPS64MSB_INST_COP1X                  = 0b010011,
    MIPS64MSB_INST_BEQL                   = 0b010100,
    MIPS64MSB_INST_BNEL                   = 0b010101,
    MIPS64MSB_INST_BLEZL_POP26            = 0b010110,
    MIPS64MSB_INST_BGTZL                  = 0b010111,

    MIPS64MSB_INST_DADDI_POP30            = 0b011000,
    MIPS64MSB_INST_DADDIU                 = 0b011001,
    MIPS64MSB_INST_LDL                    = 0b011010,
    MIPS64MSB_INST_LDR                    = 0b011011,
    MIPS64MSB_INST_SPECIAL2               = 0b011100,
    MIPS64MSB_INST_JALX_DAUI              = 0b011101,
    MIPS64MSB_INST_MSA                    = 0b011110,
    MIPS64MSB_INST_SPECAIL3               = 0b011111,

    MIPS64MSB_INST_LB                     = 0b100000,
    MIPS64MSB_INST_LH                     = 0b100001,
    MIPS64MSB_INST_LWL                    = 0b100010,
    MIPS64MSB_INST_LW                     = 0b100011,
    MIPS64MSB_INST_LBU                    = 0b100100,
    MIPS64MSB_INST_LHU                    = 0b100101,
    MIPS64MSB_INST_LWR                    = 0b100110,
    MIPS64MSB_INST_LWU                    = 0b100111,

    MIPS64MSB_INST_SB                     = 0b101000,
    MIPS64MSB_INST_SHI                    = 0b101001,
    MIPS64MSB_INST_SWL                    = 0b101010,
    MIPS64MSB_INST_SW                     = 0b101011,
    MIPS64MSB_INST_SDL                    = 0b101100,
    MIPS64MSB_INST_SDR                    = 0b101101,
    MIPS64MSB_INST_SWR                    = 0b101110,
    MIPS64MSB_INST_CACHE                  = 0b101111,

    MIPS64MSB_INST_LL                     = 0b110000,
    MIPS64MSB_INST_LWC                    = 0b110001,
    MIPS64MSB_INST_LWC2_BC                = 0b110010,
    MIPS64MSB_INST_PREF                   = 0b110011,
    MIPS64MSB_INST_LLD                    = 0b110100,
    MIPS64MSB_INST_LDC                    = 0b110101,
    MIPS64MSB_INST_LDC2_BEQZC_JIC_POP66   = 0b110110,
    MIPS64MSB_INST_LD                     = 0b110111,

    MIPS64MSB_INST_SC                     = 0b111000,
    MIPS64MSB_INST_SWC                    = 0b111001,
    MIPS64MSB_INST_SWC2_BALC              = 0b111010,
    MIPS64MSB_INST_PCREL                  = 0b111011,
    MIPS64MSB_INST_SCD                    = 0b111100,
    MIPS64MSB_INST_SDC                    = 0b111101,
    MIPS64MSB_INST_SDC2_BNEZC_JIALC_POP76 = 0b111110,
    MIPS64MSB_INST_SD                     = 0b111111,
} enum_mips64msb_opcodes_t;

/* ========================================================================== */
/*                           Register functions                               */
/* ========================================================================== */

static uint64_t
mips64msb_get_reg(const mips64msb_t* mips, enum_mips64msb_reg_t reg)
{
    return mips->registers[reg];
}

static void
mips64msb_set_reg(mips64msb_t* mips, enum_mips64msb_reg_t reg, const uint64_t value)
{
    mips->registers[reg] = value;
}

static uint64_t
mips64msb_get_pc(const mips64msb_t* mips)
{
    return mips->registers[MIPS64MSB_REG_PC];
}

static void
mips64msb_set_pc(mips64msb_t* mips, const uint64_t value)
{
    mips->registers[MIPS64MSB_REG_PC] = value;
}

static uint64_t
mips64msb_get_sp(mips64msb_t* mips)
{
    return mips->registers[MIPS64MSB_REG_R29];
}

static void
mips64msb_set_sp(mips64msb_t* mips, const uint64_t value)
{
    mips->registers[MIPS64MSB_REG_R29] = value;
}

/* ========================================================================== */
/*                           Emulator init functions                          */
/* ========================================================================== */

static void
mips64msb_stack_push(mips64msb_t* mips, uint8_t bytes[], size_t nb_bytes)
{
    const uint8_t write_ok = mips->mmu->write(mips->mmu, mips->registers[MIPS64MSB_REG_R29] - nb_bytes, bytes, nb_bytes);
    if (write_ok != 0) {
        mips->exit_reason = EMU_EXIT_REASON_SEGFAULT_WRITE;
        return;
    }
    mips->registers[MIPS64MSB_REG_R29] -= nb_bytes;
}

static void
mips64msb_load_elf(mips64msb_t* mips, const target_t* target)
{
    if (target->elf->data_length > mips->mmu->memory_size) {
        abort();
    }
    mips64msb_set_pc(mips, target->elf->entry_point);

    // Load the loadable program headers into guest memory.
    for (int i = 0; i < target->elf->nb_program_headers; i++) {
        const program_header_t* curr_prg_hdr = target->elf->program_headers[i];

        if (curr_prg_hdr->type != PROGRAM_HEADER_TYPE_LOAD) {
            continue;
        }

        // Allocate memory mapping for current, loadable program header.
        mips->mmu->nb_adr_maps++;
        mips->mmu->adr_maps = realloc(mips->mmu->adr_maps, sizeof(adr_map_t*) * mips->mmu->nb_adr_maps);
        mips->mmu->adr_maps[mips->mmu->nb_adr_maps - 1] = adr_map_create(curr_prg_hdr);

        // Sanity check.
        if ((curr_prg_hdr->virtual_address + curr_prg_hdr->file_size) > (mips->mmu->memory_size - 1)) {
            ginger_log(ERROR, "[%s] Error! Write of 0x%lx bytes to address 0x%lx "
                    "would cause write outside of emulator memory!\n", __func__,
                    curr_prg_hdr->file_size, curr_prg_hdr->virtual_address);
            abort();
        }
        mips->mmu->set_permissions(mips->mmu, curr_prg_hdr->virtual_address, MMU_PERM_WRITE, curr_prg_hdr->memory_size);
        mips->mmu->write(mips->mmu, curr_prg_hdr->virtual_address, &target->elf->data[curr_prg_hdr->offset], curr_prg_hdr->file_size);

        // Fill padding with zeros.
        const int padding_len = curr_prg_hdr->memory_size - curr_prg_hdr->file_size;
        if (padding_len > 0) {
            uint8_t padding[padding_len];
            memset(padding, 0, padding_len);
            mips->mmu->write(mips->mmu, curr_prg_hdr->virtual_address + curr_prg_hdr->file_size, padding, padding_len);
        }

        mips->mmu->set_permissions(mips->mmu, curr_prg_hdr->virtual_address, curr_prg_hdr->flags, curr_prg_hdr->memory_size);
        const uint64_t program_hdr_end = ((curr_prg_hdr->virtual_address + curr_prg_hdr->memory_size) + 0xfff) & ~0xfff;
        if (program_hdr_end > mips->mmu->curr_alloc_adr) {
            mips->mmu->curr_alloc_adr = program_hdr_end;
        }
        ginger_log(INFO, "Wrote program header %lu of size 0x%lx to guest address 0x%lx with perms ", i,
                   curr_prg_hdr->file_size, curr_prg_hdr->virtual_address);
        print_permissions(curr_prg_hdr->flags);
        printf("\n");
    }
}

static void
mips64msb_build_stack(mips64msb_t* mips, const target_t* target)
{
    // Create a stack which starts at the curr_alloc_adr of the emulator.
    // Stack is 1MiB.
    uint8_t alloc_error = 0;
    const uint64_t stack_start = mips->mmu->allocate(mips->mmu, mips->stack_size, &alloc_error);
    if (alloc_error != 0) {
        ginger_log(ERROR, "Failed allocate memory for stack!\n");
    }

    // Note the initial stack addresses in the MMU.
    const uint64_t program_header_delta = mips->mmu->adr_maps[mips->mmu->nb_adr_maps - 1]->high - mips->mmu->adr_maps[0]->low;
    mips->mmu->initial_stack_adr_mapped = program_header_delta + mips->stack_size;
    mips->mmu->initial_stack_adr_virt   = stack_start;

    // Stack grows downwards, so we set the stack pointer to starting address of the
    // stack + the stack size. As variables are allocated on the stack, their size
    // is subtracted from the stack pointer.
    mips64msb_set_sp(mips, stack_start + mips->stack_size);

    ginger_log(INFO, "Stack start: 0x%lx\n", stack_start);
    ginger_log(INFO, "Stack size:  0x%lx\n", mips->stack_size);
    ginger_log(INFO, "Stack ptr:   0x%lx\n", mips64msb_get_sp(mips));

    // Where the arguments got written to in guest memory is saved in this array.
    uint64_t guest_arg_addresses[target->argc];
    memset(&guest_arg_addresses, 0, sizeof(guest_arg_addresses));

    // Write all provided arguments into guest memory.
    for (int i = 0; i < target->argc; i++) {
        // Populate program name memory segment.
        const uint64_t arg_adr = mips->mmu->allocate(mips->mmu, ARG_MAX, &alloc_error);
        if (alloc_error != 0) {
            ginger_log(ERROR, "Failed allocate memory for target program argument!\n");
        }
        guest_arg_addresses[i] = arg_adr;
        mips->mmu->write(mips->mmu, arg_adr, (uint8_t*)target->argv[i].string, target->argv[i].length);

        // Make arg segment read and writeable.
        mips->mmu->set_permissions(mips->mmu, arg_adr, MMU_PERM_READ | MMU_PERM_WRITE, ARG_MAX);

        ginger_log(INFO, "arg[%d] \"%s\" written to guest adr: 0x%lx\n", i, target->argv[i].string, arg_adr);
    }

    ginger_log(INFO, "Building initial stack at guest address: 0x%x\n", mips64msb_get_sp(mips));

    // Push the dummy values filled with zero onto the stack as 64 bit values.
    uint8_t auxp[8]     = {0};
    uint8_t envp[8]     = {0};
    uint8_t argv_end[8] = {0};
    mips64msb_stack_push(mips, auxp, 8);
    mips64msb_stack_push(mips, envp, 8);
    mips64msb_stack_push(mips, argv_end, 8);

    // Push the guest addresses of the program arguments onto the stack.
    for (int i = target->argc - 1; i >= 0; i--) {
        uint8_t arg_buf[8] = {0};
        u64_to_byte_arr(guest_arg_addresses[i], arg_buf, ENUM_ENDIANESS_MSB);
        mips64msb_stack_push(mips, arg_buf, 8);
    }

    // Push argc onto the stack.
    uint8_t argc_buf[8] = {0};
    u64_to_byte_arr(target->argc, argc_buf, ENUM_ENDIANESS_MSB);
    mips64msb_stack_push(mips, argc_buf, 8);
}

/* ========================================================================== */
/*                           Instruction decoding functions                   */
/* ========================================================================== */

static uint8_t
inst_get_opcode(const uint32_t instruction)
{
    return instruction >> 26;
}

static inline uint8_t
inst_get_rt(uint32_t inst)
{
    return (inst >> 16) & 0b11111;
}

static inline uint8_t
inst_get_rs(uint32_t inst)
{
    return (inst >> 21) & 0b11111;
}

static inline uint8_t
inst_get_rd(uint32_t inst)
{
    return (inst >> 11) & 0b11111;
}

static uint8_t
inst_get_special(uint32_t inst)
{
    return inst & 0b111111;
}

static uint16_t
inst_get_immediate(uint32_t inst)
{
    // Lowest 16 bits.
    return inst & 0xffff;
}

static inline uint8_t
inst_get_base(uint32_t inst)
{
    return (inst >> 21) & 0b11111;
}

static inline uint16_t
inst_get_offset(uint32_t inst)
{
    // Lowest 16 bits.
    return inst & 0xffff;
}

/* ========================================================================== */
/*                           Instruction functions                            */
/* ========================================================================== */

static void
inst_lui(mips64msb_t* mips, uint32_t inst)
{
    const uint8_t  rt  = inst_get_rt(inst);
    const uint16_t imm = inst_get_immediate(inst);
    const uint64_t res = imm << 16; // Right shift preserves signedness.

    ginger_log(DEBUG, "LUI rt: %u, immediate: %u, res: %u\n", rt, imm, res);

    mips->registers[rt] = res;
    mips->registers[MIPS64MSB_REG_PC] += 4;
}

static void
inst_addiu(mips64msb_t* mips, uint32_t inst)
{
    const uint8_t  rs  = inst_get_rs(inst);
    const uint8_t  rt  = inst_get_rt(inst);
    const int16_t  imm = inst_get_immediate(inst);
    const uint32_t res = mips->registers[rs] + imm;

    ginger_log(DEBUG, "ADDIU rs: %u, rt: %u, imm %u, res: %u\n", rs, rt, imm, res);

    mips->registers[rt] = res;
    mips->registers[MIPS64MSB_REG_PC] += 4;
}

static void
inst_or(mips64msb_t* mips, uint32_t inst)
{
    const uint8_t  rs  = inst_get_rs(inst);
    const uint8_t  rt  = inst_get_rt(inst);
    const uint8_t  rd  = inst_get_rd(inst);
    const uint64_t res = mips->registers[rs] | mips->registers[rt];

    ginger_log(DEBUG, "OR rs: %u, rt: %u, rd %u, res: %u\n", rs, rt, rd, res);

    mips->registers[rd] = res;
    mips->registers[MIPS64MSB_REG_PC] += 4;
}

static void
inst_and(mips64msb_t* mips, uint32_t inst)
{
    const uint8_t  rs  = inst_get_rs(inst);
    const uint8_t  rt  = inst_get_rt(inst);
    const uint8_t  rd  = inst_get_rd(inst);
    const uint64_t res = mips->registers[rs] & mips->registers[rt];

    ginger_log(DEBUG, "AND rs: %u, rt: %u, rd %u, res: %u\n", rs, rt, rd, res);

    mips->registers[rd] = res;
    mips->registers[MIPS64MSB_REG_PC] += 4;
}

static void
inst_lw(mips64msb_t* mips, uint32_t inst)
{
    const uint8_t  base      = inst_get_base(inst);
    const uint8_t  rt        = inst_get_rt(inst);
    const uint16_t offset    = inst_get_offset(inst);
    // Risk of needing to fix signedness of following addition.
    const uint64_t target    = mips->registers[base + offset];
    uint8_t        loaded[4] = {0};

    const uint8_t ok = mips->mmu->read(mips->mmu, loaded, target, 4);

    if (ok != 0) {
        mips->exit_reason = EMU_EXIT_REASON_SEGFAULT_READ;
        return;
    }
    const uint32_t res = byte_arr_to_u64(loaded, 4, ENUM_ENDIANESS_MSB);

    ginger_log(DEBUG, "LW base: %u, rt: %u, offset: %u, target: %lu, res: %u\n", base, rt, offset, target, res);

    mips->registers[rt] = res;
    mips->registers[MIPS64MSB_REG_PC] += 4;
}

static void
inst_sw(mips64msb_t* mips, uint32_t inst)
{
    const uint8_t  base     = inst_get_base(inst);
    const uint8_t  rt       = inst_get_rt(inst);
    const uint16_t offset   = inst_get_offset(inst);
    const uint64_t target   = mips->registers[base] + offset;
    uint8_t        store[8] = {0};

    u64_to_byte_arr(mips->registers[rt] & 0xffffffff, store, ENUM_ENDIANESS_MSB);
    const uint8_t ok = mips->mmu->write(mips->mmu, target, store, 4);
    if (ok != 0) {
        mips->exit_reason = EMU_EXIT_REASON_SEGFAULT_WRITE;
        return;
    }

    ginger_log(DEBUG, "SW base: %u, rt: %u, offset: %u, target: %u\n", base, rt, offset, target);
    mips->registers[MIPS64MSB_REG_PC] += 4;
}

static void
inst_special(mips64msb_t* mips, uint32_t inst)
{
    const uint8_t special = inst_get_special(inst);
    switch (special)
    {
    // OR
    case 0b100101:
        inst_or(mips, inst);
        break;
    //AND
    case 0b100100:
        inst_and(mips, inst);
        break;
    default:
        ginger_log(ERROR, "Unimplemented special function: ");
        u8_binary_print(special);
        printf("\n");
        abort();
        break;
    }
}

/* ========================================================================== */
/*                           Emulator functions                               */
/* ========================================================================== */

static uint32_t
mips64msb_get_next_instruction(mips64msb_t* mips)
{
    uint8_t instruction_bytes[4] = {0};
    for (int i = 0; i < 4; i++) {
        uint64_t byte_adr = mips->registers[MIPS64MSB_REG_PC] + i;

        // Check if exec permission is set for this byte.
        const uint8_t current_permission = mips->mmu->permissions[byte_adr];
        if ((current_permission & MMU_PERM_EXEC) == 0) {
            ginger_log(ERROR, "No exec perm set on address: 0x%x\n", byte_adr);

            // TODO: Set emulator exit code and return here.
            abort();
        }
        instruction_bytes[i] = mips->mmu->memory[byte_adr];
    }
    return byte_arr_to_u64(instruction_bytes, 4, ENUM_ENDIANESS_MSB);
}

static bool
mips64msb_validate_opcode(mips64msb_t* mips, const uint8_t opcode)
{
    if (mips->instructions[opcode] != 0) {
        return true;
    }
    else {
        return false;
    }
}

static void
mips64msb_execute_next_instruction(mips64msb_t* mips)
{
    mips->registers[MIPS64MSB_REG_R0] = 0;

    const uint32_t instruction = mips64msb_get_next_instruction(mips);
    const uint8_t  opcode      = inst_get_opcode(instruction);

    ginger_log(DEBUG, "=========================\n");
    ginger_log(DEBUG, "PC: 0x%x\n", mips64msb_get_pc(mips));
    ginger_log(DEBUG, "Instruction\t0x%08x\n", instruction);
    ginger_log(DEBUG, "Opcode\t\t0x%x\n", opcode);

    u8_binary_print(opcode);
    printf("\n");

    if (!mips64msb_validate_opcode(mips, opcode)) {

        ginger_log(ERROR, "Invalid opcode: ");
        u8_binary_print(opcode);
        printf("\n");

        ginger_log(ERROR, "Instruction: ");
        u32_binary_print(instruction);
        printf("\n");

        mips->exit_reason = EMU_EXIT_REASON_INVALID_OPCODE;
        return;
    }

    // Execute the instruction.
    mips->instructions[opcode](mips, instruction);
}

static mips64msb_t*
mips64msb_fork(const mips64msb_t* mips)
{
    mips64msb_t* forked = mips64msb_create(mips->mmu->memory_size, mips->corpus);
    if (!forked) {
        ginger_log(ERROR, "[%s] Failed to fork mips!\n", __func__);
        abort();
    }

    // Copy emulator state.
    memcpy(forked->registers,        mips->registers,        sizeof(forked->registers));
    memcpy(forked->mmu->memory,      mips->mmu->memory,      forked->mmu->memory_size);
    memcpy(forked->mmu->permissions, mips->mmu->permissions, forked->mmu->memory_size);

    // Set the current allocation address.
    forked->mmu->curr_alloc_adr = mips->mmu->curr_alloc_adr;

    return forked;
}

static void
mips64msb_reset(mips64msb_t* dst, const mips64msb_t* src)
{
    // Reset the dirty blocks in memory.
    for (uint64_t i = 0; i < dst->mmu->dirty_state->nb_dirty_blocks; i++) {

        const uint64_t block     = dst->mmu->dirty_state->dirty_blocks[i];
        const uint64_t block_adr = block * DIRTY_BLOCK_SIZE;

        memcpy(dst->mmu->memory +      block_adr, src->mmu->memory +      block_adr, DIRTY_BLOCK_SIZE);
        memcpy(dst->mmu->permissions + block_adr, src->mmu->permissions + block_adr, DIRTY_BLOCK_SIZE);

        // Reset the allocation pointer.
        dst->mmu->curr_alloc_adr = src->mmu->curr_alloc_adr;

        // Clear the dirty bitmap.
        dst->mmu->dirty_state->dirty_bitmap[block / 64] = 0;
    }
    dst->mmu->dirty_state->clear(dst->mmu->dirty_state);

    // Reset register state.
    // TODO: This memcpy almost triples the reset time. Optimize.
    memcpy(dst->registers, src->registers, sizeof(dst->registers));

    dst->exit_reason  = EMU_EXIT_REASON_NO_EXIT;
    dst->new_coverage = false;
}

// Run an emulator until it exits or crashes.
static enum_emu_exit_reasons_t
mips64msb_run(mips64msb_t* mips, emu_stats_t* stats)
{
    // Execute the next instruction as long as no exit reason is set.
    while (mips->exit_reason == EMU_EXIT_REASON_NO_EXIT) {
        mips64msb_execute_next_instruction(mips);
        emu_stats_inc(stats, EMU_COUNTERS_EXECUTED_INSTRUCTIONS);
    }
    // Report why emulator exited.
    emu_stats_report_exit_reason(stats, mips->exit_reason);
    return mips->exit_reason;
}

static enum_emu_exit_reasons_t
mips64msb_run_until(mips64msb_t* mips, emu_stats_t* stats, const uint64_t break_adr)
{
    while (mips->exit_reason == EMU_EXIT_REASON_NO_EXIT && mips64msb_get_pc(mips) != break_adr) {
        mips64msb_execute_next_instruction(mips);
        emu_stats_inc(stats, EMU_COUNTERS_EXECUTED_INSTRUCTIONS);
    }
    // If we exited, crashed or encountered unknown behavior, report it.
    if (mips->exit_reason == EMU_EXIT_REASON_NO_EXIT) {
        emu_stats_report_exit_reason(stats, mips->exit_reason);
    }
    return mips->exit_reason;
}

static void
mips64msb_print_registers(mips64msb_t* mips)
{
    printf("\n");
    printf("R0\t0x%lx\n", mips->registers[MIPS64MSB_REG_R0]);
    printf("R1\t0x%lx\n", mips->registers[MIPS64MSB_REG_R1]);
    printf("R2\t0x%lx\n", mips->registers[MIPS64MSB_REG_R2]);
    printf("R3\t0x%lx\n", mips->registers[MIPS64MSB_REG_R3]);
    printf("R4\t0x%lx\n", mips->registers[MIPS64MSB_REG_R4]);
    printf("R5\t0x%lx\n", mips->registers[MIPS64MSB_REG_R5]);
    printf("R6\t0x%lx\n", mips->registers[MIPS64MSB_REG_R6]);
    printf("R7\t0x%lx\n", mips->registers[MIPS64MSB_REG_R7]);
    printf("R8\t0x%lx\n", mips->registers[MIPS64MSB_REG_R8]);
    printf("R9\t0x%lx\n", mips->registers[MIPS64MSB_REG_R9]);
    printf("R10\t0x%lx\n", mips->registers[MIPS64MSB_REG_R10]);
    printf("R11\t0x%lx\n", mips->registers[MIPS64MSB_REG_R11]);
    printf("R12\t0x%lx\n", mips->registers[MIPS64MSB_REG_R12]);
    printf("R13\t0x%lx\n", mips->registers[MIPS64MSB_REG_R13]);
    printf("R14\t0x%lx\n", mips->registers[MIPS64MSB_REG_R14]);
    printf("R15\t0x%lx\n", mips->registers[MIPS64MSB_REG_R15]);
    printf("R16\t0x%lx\n", mips->registers[MIPS64MSB_REG_R16]);
    printf("R17\t0x%lx\n", mips->registers[MIPS64MSB_REG_R17]);
    printf("R18\t0x%lx\n", mips->registers[MIPS64MSB_REG_R18]);
    printf("R19\t0x%lx\n", mips->registers[MIPS64MSB_REG_R19]);
    printf("R20\t0x%lx\n", mips->registers[MIPS64MSB_REG_R20]);
    printf("R21\t0x%lx\n", mips->registers[MIPS64MSB_REG_R21]);
    printf("R22\t0x%lx\n", mips->registers[MIPS64MSB_REG_R22]);
    printf("R23\t0x%lx\n", mips->registers[MIPS64MSB_REG_R23]);
    printf("R24\t0x%lx\n", mips->registers[MIPS64MSB_REG_R24]);
    printf("R25\t0x%lx\n", mips->registers[MIPS64MSB_REG_R25]);
    printf("R26\t0x%lx\n", mips->registers[MIPS64MSB_REG_R26]);
    printf("R27\t0x%lx\n", mips->registers[MIPS64MSB_REG_R27]);
    printf("R28\t0x%lx\n", mips->registers[MIPS64MSB_REG_R28]);
    printf("R29\t0x%lx\n", mips->registers[MIPS64MSB_REG_R29]);
    printf("R30\t0x%lx\n", mips->registers[MIPS64MSB_REG_R30]);
    printf("R31\t0x%lx\n", mips->registers[MIPS64MSB_REG_R31]);
    printf("HI\t0x%lx\n", mips->registers[MIPS64MSB_REG_HI]);
    printf("LO\t0x%lx\n", mips->registers[MIPS64MSB_REG_LO]);
    printf("PC\t0x%lx\n", mips->registers[MIPS64MSB_REG_PC]);
}

mips64msb_t*
mips64msb_create(size_t memory_size, corpus_t* corpus)
{
    mips64msb_t* mips = calloc(1, sizeof(mips64msb_t));
    if (!mips) {
        ginger_log(ERROR, "[%s] Could not create MIPS emulator!\n", __func__);
        return NULL;
    }

    // 1MiB stack.
    mips->stack_size = 1024 * 1024;
    mips->mmu = mmu_create(memory_size, mips->stack_size);

    if (!mips->mmu) {
        ginger_log(ERROR, "[%s] Could not create MMU!\n", __func__);
        abort();
    }

    // API.
    mips->load_elf    = mips64msb_load_elf;
    mips->build_stack = mips64msb_build_stack;
    mips->execute     = mips64msb_execute_next_instruction;
    mips->fork        = mips64msb_fork;
    mips->reset       = mips64msb_reset;
    mips->run         = mips64msb_run;
    mips->run_until   = mips64msb_run_until;
    mips->stack_push  = mips64msb_stack_push;
    mips->print_regs  = mips64msb_print_registers;
    mips->get_pc      = mips64msb_get_pc;
    mips->get_reg     = mips64msb_get_reg;
    mips->set_reg     = mips64msb_set_reg;

    mips->exit_reason  = EMU_EXIT_REASON_NO_EXIT;
    mips->new_coverage = false;
    mips->corpus       = corpus;

    mips->instructions[MIPS64MSB_INST_LUI]     = inst_lui;
    mips->instructions[MIPS64MSB_INST_ADDIU]   = inst_addiu;
    mips->instructions[MIPS64MSB_INST_SPECIAL] = inst_special;
    mips->instructions[MIPS64MSB_INST_LW]      = inst_lw;
    mips->instructions[MIPS64MSB_INST_SW]      = inst_sw;

    return mips;
}

void
mips64msb_destroy(mips64msb_t* mips)
{
    if (mips) {
        if (mips->mmu) {
            mmu_destroy(mips->mmu);
        }
        free(mips);
    }
}
