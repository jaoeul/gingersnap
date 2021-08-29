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

struct emu_exit_counters {
    uint64_t unsupported_syscall;
    uint64_t fstat_bad_fd;
    uint64_t unknown_exit_reason;
};

// TODO: Atomic increments.
//       Globas stats struct.
struct emu_exit_counters g_counter;

static void
inc_exit_counter(const int counter)
{
    switch(counter) {
        case EMU_EXIT_REASON_SYSCALL_NOT_SUPPORTED:
            g_counter.unsupported_syscall++;
            break;

        case EMU_EXIT_FSTAT_BAD_FD:
            g_counter.fstat_bad_fd++;
            break;

        default:
            g_counter.unknown_exit_reason++;
            break;
    }
}

static void
run_emu(risc_v_emu_t* emu, cli_t* cli, const bool debug)
{
    for (;;) {
        // Exit reason. Set when emulator exits.
        if (emu->exit_reason == EMU_EXIT_REASON_NO_EXIT) {

            // FIXME: Debug does not halt on unknown syscall.
            if (debug) {
                debug_emu(emu, cli);
            }
            emu->execute(emu);
        }
        // Handle emu exit and break out of the run loop.
        else {
            inc_exit_counter(emu->exit_reason);
            break;
        }
    }
}

int
main(int argc, char** argv)
{
    const size_t  emu_total_mem   = (1024 * 1024) * 256;
    risc_v_emu_t* emu             = risc_v_emu_create(emu_total_mem);
    const char    target_name[]   = "target";
    const size_t  target_name_len = strlen(target_name);

    // TODO: Support multiple target arguments.
    //       Do not use any arguments for now.
    const int     target_argc     = 1;
    const char    target_arg[]    = {0};
    const size_t  target_arg_len  = strlen(target_arg);

    // Load the elf into emulator memory.
    // Virtual addresses in the elf program headers corresponds directly to
    // addresses in the emulators memory.
    // TODO: getopt
    if (argv[1] == NULL) {
        abort();
    }
    load_elf(argv[1], emu);
    ginger_log(INFO, "curr_alloc_adr: 0x%lx\n", emu->mmu->curr_alloc_adr);

    // Create a stack which starts at the curr_alloc_adr of the emulator.
    // Stack is 1MiB.
    const uint64_t stack_start = emu->mmu->allocate(emu->mmu, emu->stack_size);

    // Stack grows downwards, so we set the stack pointer to starting address of the
    // stack + the stack size. As variables are allocated on the stack, their size
    // is subtracted from the stack pointer.
    emu->registers[REG_SP] = stack_start + emu->stack_size;

    ginger_log(INFO, "Stack start: 0x%lx\n", stack_start);
    ginger_log(INFO, "Stack size:  0x%lx\n", emu->stack_size);
    ginger_log(INFO, "Stack ptr:   0x%lx\n", emu->registers[REG_SP]);

    // Populate program name memory segment.
    uint64_t program_name_adr = emu->mmu->allocate(emu->mmu, 4096);
    ginger_log(INFO, "program_name_adr: 0x%lx\n", program_name_adr);
    emu->mmu->write(emu->mmu, program_name_adr, (uint8_t*)target_name, target_name_len);

    // Populate arg1 name memory segment.
    uint64_t argv1_adr = emu->mmu->allocate(emu->mmu, 4096);
    ginger_log(INFO, "argv1_adr: 0x%lx\n", argv1_adr);
    emu->mmu->write(emu->mmu, argv1_adr, (uint8_t*)target_arg, target_arg_len);

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
    u64_to_byte_arr(target_argc, arg_c, LSB); // Number of arguments to the target executable.

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
#ifdef AUTO_DEBUG
    const bool debug = true;
#else
    const bool debug = false;
#endif
    run_emu(emu, debug_cli, debug);

    ginger_log(INFO, "Freeing allocated data!\n");
    cli_destroy(debug_cli);
    emu->destroy(emu);

    printf("\nCounters\n");
    printf("unsupported_syscall: %lu\n", g_counter.unsupported_syscall);
    printf("fstat_bad_fd: %lu\n", g_counter.fstat_bad_fd);
    printf("unknown_exit_reason: %lu\n", g_counter.unknown_exit_reason);

    return 0;
}
