#include <malloc.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h> // usleep

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
    pthread_t       thread_id;  // ID returned by pthread_create().
    uint64_t        thread_num; // Application-defined thread number.
    rv_emu_t*       emu;        // The emulator to run.
    const target_t* target;     // The target executable.
};

// Global variables are zeroed out at program start, so we do not need to do it
// manually.
struct emu_exit_counters g_counter;

// Atomically increment counters.
static void
inc_exit_counter(const int counter)
{
    switch(counter) {
        case EMU_EXIT_REASON_SYSCALL_NOT_SUPPORTED:
            __atomic_fetch_add(&g_counter.unsupported_syscall, 1, __ATOMIC_SEQ_CST);
            break;
        case EMU_EXIT_FSTAT_BAD_FD:
            __atomic_fetch_add(&g_counter.fstat_bad_fd, 1, __ATOMIC_SEQ_CST);
            break;
        case EMU_EXIT_GRACEFUL:
            __atomic_fetch_add(&g_counter.graceful_exit, 1, __ATOMIC_SEQ_CST);
            break;
        default:
            __atomic_fetch_add(&g_counter.unknown_exit_reason, 1, __ATOMIC_SEQ_CST);
            break;
    }
}

// Parse `/proc/cpuinfo`.
// TODO: Create string/file parsing lib.
uint8_t
nb_active_cpus(void)
{
    uint8_t nb_cpus = 0;
    FILE* fp = fopen("/proc/cpuinfo", "rb");
    if (!fp) {
        ginger_log(ERROR, "Could not open /proc/cpuinfo\n");
        abort();
    }

    char*   line = NULL;
    size_t  len  = 0;
    ssize_t read = 0;
    while ((read = getline(&line, &len, fp)) != -1) {
        if (strstr(line, "processor")) {
            nb_cpus++;
        }
    }
    return nb_cpus;
}

// Run an emulator until it exits or crashes.
static void
run_emu(rv_emu_t* emu)
{
#if EMU_DEBUG
    const pid_t actual_tid = syscall(__NR_gettid);
    if (emu->tid != actual_tid) {
        ginger_log(ERROR, "[%s] Thread tried to execute someone elses emu!\n", __func__);
        ginger_log(ERROR, "[%s] acutal_tid 0x%x, emu->tid: 0x%x\n", __func__, actual_tid, emu->tid);
        abort();
    }
#endif
    // Execute the next instruction as long as no exit reason is set.
    while (emu->exit_reason == EMU_EXIT_REASON_NO_EXIT) {
        emu->execute(emu);
    }

    // Report why emulator exited.
    inc_exit_counter(emu->exit_reason);
}

// Continously spawn new emulators when they exit or crash.
static void*
thread_run(void* arg)
{
    struct thread_info* t_info = arg;
    rv_emu_t*           emu    = t_info->emu;
    const target_t*     target = t_info->target;

    // Init the emulator thread id.
    emu->tid = syscall(__NR_gettid);

    // Load the elf and build the stack.
    emu->setup(emu, target);

    // Capture the state.
    const rv_emu_t* clean_snapshot = emu->fork(emu);

    for (int i = 0; i < 100000; i++) {
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
    const size_t  rv_emu_total_mem = (1024 * 1024) * 256;
    const int     target_argc      = 1;

    // Array of arguments to the target executable.
    heap_str_t target_argv[target_argc];
    memset(target_argv, 0, sizeof(target_argv));

    // First arg.
    heap_str_t arg0;
    heap_str_set(&arg0, "./data/target");
    target_argv[0] = arg0;

    // Multiple emus can use the same target since it is only read from and never written to.
    const target_t* target = target_create(target_argc, target_argv);

    const uint8_t nb_cpus = nb_active_cpus();
    ginger_log(INFO, "Number active cpus: %u\n", nb_cpus);

    // Can be used for all threads.
    pthread_attr_t thread_attr = {0};
    pthread_attr_init(&thread_attr);

    // Per-thread info. Multiple threads should never use the same emulator.
    struct thread_info t_info[nb_cpus];
    memset(t_info, 0, sizeof(t_info));

    // Per thread emulators.
    rv_emu_t* emus[nb_cpus];
    memset(emus, 0, sizeof(emus));

    // Start one emulator per available cpu.
    for (uint8_t i = 0; i < nb_cpus; i++) {
        // Create an emulator for current thread.
        emus[i] = emu_create(rv_emu_total_mem);

        t_info[i].thread_num = i;
        t_info[i].emu        = emus[i];
        t_info[i].target     = target;

        const int ok = pthread_create(&t_info[i].thread_id, &thread_attr, &thread_run, &t_info[i]);
        if (ok != 0) {
            ginger_log(ERROR, "[%s] Failed spawn thread!\n", __func__);
            abort();
        }
        printf("t_info[i].thread_id  after  0x%lx\n", t_info[i].thread_id);
        printf("\n");
    }

    // We are done with the attributes, might as well destroy them.
    int ok = pthread_attr_destroy(&thread_attr);
    if (ok != 0) abort();

    // Wait for all threads to finish.
    for (uint8_t i = 0; i < nb_cpus; i++) {
        void* thread_ret = NULL;
        ok               = pthread_join(t_info[i].thread_id, &thread_ret);
        if (ok != 0) {
            abort();
        }
        free(thread_ret);
        emus[i]->destroy(emus[i]);
    }
    ginger_log(INFO, "All threads joined. Freeing allocated data!\n");

    // Create debugging cli.
    //cli_t* debug_cli = emu_debug_create_cli(emu);

    // Start the emulator.
//#ifdef AUTO_DEBUG
    //const bool debug = true;
//#else
    //const bool debug = false;
//#endif
    //(void)debug;

    target_destroy((void*)target);
    //cli_destroy(debug_cli);

    printf("\nCounters\n");
    printf("unsupported_syscall: %lu\n", g_counter.unsupported_syscall);
    printf("fstat_bad_fd: %lu\n", g_counter.fstat_bad_fd);
    printf("unknown_exit_reason: %lu\n", g_counter.unknown_exit_reason);
    printf("graceful_exit: %lu\n", g_counter.graceful_exit);
    /*
    */

    return 0;
}
