#include <malloc.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../emu/riscv_emu.h"
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

// Used as argument to the threads running the emulators.
struct thread_info {
    pthread_t     thread_id;  // ID returned by pthread_create().
    uint64_t      thread_num; // Application-defined thread number.
    rv_emu_t* emu;        // The emulator to run.
    target_t*     target;     // The target executable.
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

// Run an emulator until it exits or crashes.
__attribute__((used))
static void
run_emu(rv_emu_t* emu)
{
    // Execute the next instruction as long as no exit reason is set.
    while (emu->exit_reason == EMU_EXIT_REASON_NO_EXIT) {
        emu->execute(emu);
    }

    // Report why emulator exited.
    inc_exit_counter(emu->exit_reason);
}

// Continously spawn new emulators when they exit or crash.
__attribute__((used))
static void*
thread_run(void* arg)
{
    struct thread_info* t_info = arg;
    rv_emu_t*    emu    = t_info->emu;
    target_t* target = t_info->target;

    // Load the elf and build the stack.
    emu->setup(emu, target);

    // Capture the state.
    const rv_emu_t* clean_snapshot = emu->fork(emu);

    for (int i = 0; i < 1000000000; i++) {

        // Run the emulator until it exits or crashes.
        run_emu(emu);

        // Restore the emulator to its initial state.
        emu->reset(emu, clean_snapshot);
    }

    return NULL;
}

int
main(int argc, char** argv)
{
    // Create one emulator per active thread.
    const size_t  rv_emu_total_mem  = (1024 * 1024) * 256;
    rv_emu_t* emu            = emu_create(rv_emu_total_mem);
    const int     target_argc    = 1;

    // Array of arguments to the target executable.
    heap_str_t target_argv[target_argc];
    memset(target_argv, 0, sizeof(target_argv));

    // First arg.
    heap_str_t arg0;
    heap_str_set(&arg0, "./target");
    target_argv[0] = arg0;

    // Prepare the target executable.
    target_t* target = target_create(target_argc, target_argv);

    ginger_log(INFO, "curr_alloc_adr: 0x%lx\n", emu->mmu->curr_alloc_adr);
    ginger_log(INFO, "Current allocation address: 0x%lx\n", emu->mmu->curr_alloc_adr);

    // Can be used for all threads.
    pthread_attr_t thread_attr = {0};
    pthread_attr_init(&thread_attr);

    // Set the desired thread attributes here.

    // Per-thread info. Multiple threads should never use the same emulator.
    struct thread_info t_info = {
        .thread_num = 1,
        .emu        = emu,
        .target     = target,
    };

    int ok = pthread_create(&t_info.thread_id, &thread_attr, &thread_run, &t_info);
    if (ok != 0) abort();

    ok = pthread_attr_destroy(&thread_attr);
    if (ok != 0) abort();

    void* thread_ret = NULL;
    ok               = pthread_join(t_info.thread_id, &thread_ret);
    if (ok != 0) abort();

    free(thread_ret);


    // Create debugging cli.
    //cli_t* debug_cli = emu_debug_create_cli(emu);

    // Start the emulator.
//#ifdef AUTO_DEBUG
    //const bool debug = true;
//#else
    //const bool debug = false;
//#endif
    //(void)debug;

    ginger_log(INFO, "Freeing allocated data!\n");

    target_destroy(target);
    //cli_destroy(debug_cli);
    emu->destroy(emu);

    printf("\nCounters\n");
    printf("unsupported_syscall: %lu\n", g_counter.unsupported_syscall);
    printf("fstat_bad_fd: %lu\n", g_counter.fstat_bad_fd);
    printf("unknown_exit_reason: %lu\n", g_counter.unknown_exit_reason);
    printf("graceful_exit: %lu\n", g_counter.graceful_exit);

    return 0;
}
