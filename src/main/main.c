#include <malloc.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <time.h>
#include <unistd.h>

#include "../emu/riscv_emu.h"
#include "../emu/emu_stats.h"
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

#define cpu_relax() asm volatile("rep; nop")

// Used as argument to the threads running the emulators.
typedef struct {
    pthread_t       thread_id;    // ID returned by pthread_create().
    uint64_t        thread_num;   // Application-defined thread number.
    rv_emu_t*       emu;          // The emulator to run.
    const target_t* target;       // The target executable.
    emu_stats_t*    shared_stats; // Statistics.
} thread_info_t;

// Parse `/proc/cpuinfo`.
uint8_t
nb_active_cpus(void)
{
    uint8_t nb_cpus = 0;
    char*   line = NULL;
    size_t  len  = 0;
    ssize_t read = 0;

    FILE* fp = fopen("/proc/cpuinfo", "rb");
    if (!fp) {
        ginger_log(ERROR, "Could not open /proc/cpuinfo\n");
        abort();
    }
    while ((read = getline(&line, &len, fp)) != -1) {
        if (strstr(line, "processor")) {
            nb_cpus++;
        }
    }
    return nb_cpus;
}

// Run an emulator until it exits or crashes.
static void
run_emu(rv_emu_t* emu, emu_stats_t* local_stats)
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
        emu_stats_inc(local_stats, EMU_COUNTERS_EXECUTED_INSTRUCTIONS);
    }

    // Report why emulator exited.
    switch(emu->exit_reason) {
    case EMU_EXIT_REASON_SYSCALL_NOT_SUPPORTED:
        emu_stats_inc(local_stats, EMU_COUNTERS_EXIT_REASON_SYSCALL_NOT_SUPPORTED);
        break;
    case EMU_EXIT_REASON_FSTAT_BAD_FD:
        emu_stats_inc(local_stats, EMU_COUNTERS_EXIT_FSTAT_BAD_FD);
        break;
    case EMU_EXIT_REASON_GRACEFUL:
        emu_stats_inc(local_stats, EMU_COUNTERS_EXIT_GRACEFUL);
        break;
    case EMU_EXIT_REASON_NO_EXIT:
        break;
    }
}

// Run the emulator and reset it when it exit or crashes. Report stats from
// the thread local data to the main stats structure after a set time interval.
static void*
__attribute__((noreturn))
worker_run(void* arg)
{
    thread_info_t*  t_info       = arg;
    rv_emu_t*       emu          = t_info->emu;
    const target_t* target       = t_info->target;
    emu_stats_t*    shared_stats = t_info->shared_stats;

    // Init the emulator thread id.
    emu->tid = syscall(__NR_gettid);

    // Load the elf and build the stack.
    emu->setup(emu, target);

    // Create a thread local stats structure.
    emu_stats_t* local_stats = emu_stats_create();

    // Capture the state.
    const rv_emu_t* clean_snapshot = emu->fork(emu);

    // A timestamp which is used for comparison.
    struct timespec checkpoint;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &checkpoint);

    for (;;) {
        // Run the emulator until it exits or crashes.
        run_emu(emu, local_stats);

        // Restore the emulator to its initial state.
        emu->reset(emu, clean_snapshot);

        // Update the main stats with data from the thread local stats if the time
        // is right.
        struct timespec current;
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &current);
        const time_t elapsed_s = current.tv_sec - checkpoint.tv_sec;

        // Report stats to the main thread about once every second.
        if (elapsed_s > 1) {
            // Lock shared stats mutex.
            pthread_mutex_lock(&shared_stats->lock);
            shared_stats->nb_executed_instructions += local_stats->nb_executed_instructions;
            shared_stats->nb_unsupported_syscalls  += local_stats->nb_unsupported_syscalls;
            shared_stats->nb_fstat_bad_fds         += local_stats->nb_fstat_bad_fds;
            shared_stats->nb_graceful_exits        += local_stats->nb_graceful_exits;
            shared_stats->nb_unknown_exit_reasons  += local_stats->nb_unsupported_syscalls;
            // Unlock the mutex.
            pthread_mutex_unlock(&shared_stats->lock);

            // Reset the timer checkpoint.
            clock_gettime(CLOCK_THREAD_CPUTIME_ID, &checkpoint);

            // Clear the local stats.
            memset(local_stats, 0, sizeof(emu_stats_t));
        }
    }
}

int
main(int argc, char** argv)
{
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

    // Can be used for all threads.
    pthread_attr_t thread_attr = {0};
    pthread_attr_init(&thread_attr);

    // Per-thread info. Multiple threads should never use the same emulator.
    const uint8_t nb_cpus = nb_active_cpus();
    ginger_log(INFO, "Number active cpus: %u\n", nb_cpus);
    thread_info_t t_info[nb_cpus];
    memset(t_info, 0, sizeof(t_info));

    // Total statistics. Shared between threads. Guarded by a mutex. Updated
    // reguarly at a set time interval.
    emu_stats_t* shared_stats = emu_stats_create();

    // Start one emulator per available cpu.
    for (uint8_t i = 0; i < nb_cpus; i++) {
        t_info[i].thread_num   = i;
        t_info[i].emu          = emu_create(rv_emu_total_mem);
        t_info[i].target       = target;
        t_info[i].shared_stats = shared_stats;

        const int ok = pthread_create(&t_info[i].thread_id, &thread_attr, &worker_run, &t_info[i]);
        if (ok != 0) {
            ginger_log(ERROR, "[%s] Failed spawn thread!\n", __func__);
            abort();
        }
        printf("t_info[i].thread_id  after  0x%lx\n", t_info[i].thread_id);
        printf("\n");
    }
    // We are done with the thread attributes, might as well destroy them.
    int ok = pthread_attr_destroy(&thread_attr);
    if (ok != 0) {
        abort();
    }

    // The main threads job is displaying stats reported from the worker
    // threads. Do this once a second.
    struct timespec checkpoint;
    struct timespec current;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &checkpoint);
    for (;;) {
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &current);
        const time_t elapsed_s = current.tv_sec - checkpoint.tv_sec;
        if (elapsed_s > 1) {
            emu_stats_print(shared_stats);

            // Reset the timer checkpoint.
            clock_gettime(CLOCK_THREAD_CPUTIME_ID, &checkpoint);
        }
        else {
            cpu_relax();
        }
    }

    // Wait for all threads to finish.
    for (uint8_t i = 0; i < nb_cpus; i++) {
        void* thread_ret = NULL;
        ok               = pthread_join(t_info[i].thread_id, &thread_ret);
        if (ok != 0) {
            abort();
        }
        free(thread_ret);
        t_info[i].emu->destroy(t_info[i].emu);
    }
    ginger_log(INFO, "All threads joined. Freeing allocated data!\n");

    target_destroy((void*)target);

    return 0;
}
