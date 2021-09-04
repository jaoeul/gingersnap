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

    target_destroy(target);
    cli_destroy(debug_cli);
    emu->destroy(emu);

    printf("\nCounters\n");
    printf("unsupported_syscall: %lu\n", g_counter.unsupported_syscall);
    printf("fstat_bad_fd: %lu\n", g_counter.fstat_bad_fd);
    printf("unknown_exit_reason: %lu\n", g_counter.unknown_exit_reason);
    printf("graceful_exit: %lu\n", g_counter.graceful_exit);

    return 0;
}
