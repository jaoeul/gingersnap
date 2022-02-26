#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <syscall.h>
#include <unistd.h>
#include <pthread.h>

#include "emu_generic.h"
#include "emu_riscv.h"
#include "emu_x86_64.h"
#include "syscall.h"

#include "../corpus/coverage.h"
#include "../corpus/corpus.h"
#include "../shared/endianess_converter.h"
#include "../shared/logger.h"
//#include "../shared/target.h"
#include "../shared/vector.h"

static void
emu_generic_load_elf(emu_t* emu, const target_t* target)
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
        emu->mmu->set_permissions(emu->mmu, curr_prg_hdr->virtual_address, PERM_WRITE, curr_prg_hdr->memory_size);

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
emu_generic_build_stack(emu_t* emu, const target_t* target)
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
        emu->mmu->set_permissions(emu->mmu, arg_adr, PERM_READ | PERM_WRITE, ARG_MAX);

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

emu_t*
emu_generic_create(size_t memory_size, corpus_t* corpus, enum_emu_supported_archs_t arch)
{
    emu_t* emu = NULL;

    switch (arch)
    {
        case ENUM_EMU_SUPPORTED_ARCHS_RISC_V:
            emu = emu_riscv_create(memory_size, corpus);
            break;
        case ENUM_EMU_SUPPORTED_ARCHS_X86_64:
            emu = emu_x86_64_create(memory_size, corpus);
            break;
        default:
            ginger_log(ERROR, "[%s] Usupported architecture!\n", __func__);
            abort();
    }

    if (!emu) {
        ginger_log(ERROR, "[%s] Failed to create emu!\n", __func__);
        abort();
    }

    emu->load_elf    = emu_generic_load_elf;
    emu->build_stack = emu_generic_build_stack;

    return emu;
}

void
emu_generic_destroy(emu_t* emu)
{
    switch (emu->arch)
    {
        case ENUM_EMU_SUPPORTED_ARCHS_RISC_V:
            return emu_riscv_destroy(emu);
        case ENUM_EMU_SUPPORTED_ARCHS_X86_64:
            return emu_x86_64_destroy(emu);
        default:
            ginger_log(ERROR, "[%s] Usupported architecture!\n", __func__);
            abort();
    }
}

