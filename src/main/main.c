#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../emu/risc_v_emu.h"
#include "../debug_cli/debug_cli.h"
#include "../shared/cli.h"
#include "../shared/elf_loader.h"
#include "../shared/endianess_converter.h"
#include "../shared/endianess_converter.h"
#include "../shared/logger.h"
#include "../shared/print_utils.h"
#include "../shared/vector.h"

static void
run_emu(risc_v_emu_t* emu, cli_t* cli)
{
    for (;;) {
        debug_emu(emu, cli);
        emu->execute(emu);
    }
}

int
main(int argc, char** argv)
{
    const size_t emu_total_mem = (1024 * 1024) * 256;//0x40007ffed0;
    risc_v_emu_t* emu = risc_v_emu_create(emu_total_mem);

    // Load the elf into emulator memory.
    // Virtual addresses in the elf program headers corresponds directly to
    // addresses in the emulators memory.
    // TODO: getopt
    if (argv[1] == NULL) {
        abort();
    }
    load_elf(argv[1], emu);

    // Create a stack which starts at the curr_allocation address of the emulator.
    // Stack is 1MiB.
    const size_t   stack_size  = (1024 * 1024);
    const uint64_t stack_start = emu->mmu->allocate(emu->mmu, stack_size);

    // Stack grows downwards, so we set the stack pointer to starting address of the
    // stack + the stack size. As variables are allocated on the stack, their size
    // is subtracted from the stack pointer.
    emu->registers[REG_SP] = stack_start + stack_size;

    ginger_log(INFO, "Stack start: 0x%lx\n", stack_start);
    ginger_log(INFO, "Stack size:  0x%lx\n", stack_size);
    ginger_log(INFO, "Stack ptr:   0x%lx\n", emu->registers[REG_SP]);

    // Populate program name memory segment.
    uint64_t program_name_adr = emu->mmu->allocate(emu->mmu, 4096);
    ginger_log(INFO, "program_name_adr: 0x%lx\n", program_name_adr);
    emu->mmu->write(emu->mmu, program_name_adr, (uint8_t*)"target\0", 6);

    // Populate arg1 name memory segment.
    uint64_t argv1_adr = emu->mmu->allocate(emu->mmu, 4096);
    ginger_log(INFO, "argv1_adr: 0x%lx\n", argv1_adr);
    emu->mmu->write(emu->mmu, argv1_adr, (uint8_t*)"arg1\0", 4);

    // Push initial values onto the stack
    ginger_log(INFO, "Building initial stack at guest address: 0x%x\n", emu->registers[REG_SP]);
    uint8_t auxp[8]     = {0}; // Not used.
    uint8_t envp[8]     = {0}; // Not used.
    uint8_t argv_end[8] = {0}; // Not used.
    uint8_t argv1[8]    = {0}; // Used.
    uint8_t argv0[8]    = {0}; // Used.
    uint8_t arg_c[8]    = {0}; // Used.

    u64_to_byte_arr(argv1_adr, argv1, LSB);
    u64_to_byte_arr(program_name_adr, argv0, LSB);
    u64_to_byte_arr(1, arg_c, LSB); // Number of arguments to the target executable.

    // Push the required values onto the stack as 64 bit values.
    emu->stack_push(emu, auxp, 8);     // 0u64
    emu->stack_push(emu, envp, 8);     // 0u64
    emu->stack_push(emu, argv_end, 8); // 0u64
    emu->stack_push(emu, argv1, 8);    // The guest address of the argv1_adr allocation.
    emu->stack_push(emu, argv0, 8);    // The guest address of the program_name_adr allocation.
    emu->stack_push(emu, arg_c, 8);    // Number of cli arguments.

    ginger_log(INFO, "Current allocation address: 0x%lx\n", emu->mmu->curr_alloc_adr);

    if (argv[2] != NULL) {
        print_emu_memory_all(emu);
    }

    // Create debugging cli.
    cli_t* debug_cli = emu_debug_create_cli(emu);

    // Start the emulator.
    run_emu(emu, debug_cli);

    ginger_log(INFO, "Freeing allocated data!\n");
    cli_destroy(debug_cli);
    emu->destroy(emu);

    return 0;
}
