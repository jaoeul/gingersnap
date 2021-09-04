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
#include "../shared/heap_str.h"
#include "../shared/logger.h"
#include "../shared/print_utils.h"
#include "../shared/vector.h"
#include "../target/target.h"

// Max length of an cli argument of the target executable.
#define ARG_MAX 4096

struct emu_exit_counters {
    uint64_t unsupported_syscall;
    uint64_t fstat_bad_fd;
    uint64_t unknown_exit_reason;
    uint64_t graceful_exit;
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
        case EMU_EXIT_GRACEFUL:
            g_counter.graceful_exit++;
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
    const size_t  emu_total_mem  = (1024 * 1024) * 256;
    risc_v_emu_t* emu            = risc_v_emu_create(emu_total_mem);
    const int  target_argc       = 1;

    // Array of arguments to the target executable.
    heap_str_t target_argv[target_argc];
    memset(target_argv, 0, sizeof(target_argv));

    // First arg.
    heap_str_t arg0;
    heap_str_set(&arg0, "./target");
    target_argv[0] = arg0;

    // Create the a class, representing the target executable.
    target_t* target = target_create(target_argc, target_argv);

    // Load the elf and build the stack.
    emu->setup(emu, target);

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

    // Where the arguments got written to in guest memory is saved in this array.
    uint64_t guest_arg_addresses[target->argc];
    memset(&guest_arg_addresses, 0, sizeof(guest_arg_addresses));

    // Write all provided arguments into guest memory.
    for (int i = 0; i < target->argc; i++) {
        // Populate program name memory segment.
        const uint64_t arg_adr = emu->mmu->allocate(emu->mmu, ARG_MAX);
        guest_arg_addresses[i] = arg_adr;
        emu->mmu->write(emu->mmu, arg_adr, (uint8_t*)target->argv[i].str, target->argv[i].len);
        ginger_log(INFO, "arg[%d] written to guest adr: 0x%lx\n", i, arg_adr);
    }

    ginger_log(INFO, "Building initial stack at guest address: 0x%x\n", emu->registers[REG_SP]);

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
    (void)debug;
    run_emu(emu, debug_cli, debug);

    ginger_log(INFO, "Freeing allocated data!\n");
    cli_destroy(debug_cli);
    emu->destroy(emu);

    printf("\nCounters\n");
    printf("unsupported_syscall: %lu\n", g_counter.unsupported_syscall);
    printf("fstat_bad_fd: %lu\n", g_counter.fstat_bad_fd);
    printf("unknown_exit_reason: %lu\n", g_counter.unknown_exit_reason);
    printf("graceful_exit: %lu\n", g_counter.graceful_exit);

    return 0;
}
