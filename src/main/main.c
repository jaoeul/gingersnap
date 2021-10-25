#define _GNU_SOURCE

#include <malloc.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <time.h>
#include <unistd.h>

#include "../emu/riscv_emu.h"
#include "../emu/emu_stats.h"
#include "../fuzzer/fuzzer.h"
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
    const target_t* target;       // The target executable.
    emu_stats_t*    shared_stats; // Collected statistics, reported by the worker threads.
    corpus_t*       corpus;       // Data which the fuzz inputs are based on. Shared between threads.
    uint64_t        fuzz_buf_adr;
    uint64_t        fuzz_buf_size;
    const rv_emu_t* clean_snapshot;
    const char*     crash_dir;
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

// Do the all the thread local setup of fuzzers and start them. Report stats from
// the thread local data to the main stats structure after a set time interval.
__attribute__((noreturn))
static void*
worker_run(void* arg)
{
    const uint64_t  report_stats_interval_ns = 1e7; // Report stats 100 times a second to the main thread.

    // Thread arguments.
    thread_info_t*  t_info         = arg;
    const target_t* target         = t_info->target;
    const uint64_t  fuzz_buf_adr   = t_info->fuzz_buf_adr;
    const uint64_t  fuzz_buf_size  = t_info->fuzz_buf_size;
    corpus_t*       corpus         = t_info->corpus;
    const rv_emu_t* clean_snapshot = t_info->clean_snapshot;
    emu_stats_t*    shared_stats   = t_info->shared_stats;
    const char*     crash_dir      = t_info->crash_dir;

    // Create the thread local fuzzer.
    fuzzer_t* fuzzer = fuzzer_create(corpus, fuzz_buf_adr, fuzz_buf_size, target, clean_snapshot, crash_dir);

    // A timestamp which is used for comparison.
    struct timespec checkpoint;
    clock_gettime(CLOCK_MONOTONIC, &checkpoint);

    for (;;) {
        // Run one fuzzcase.
        fuzzer->fuzz(fuzzer);

        // If we crashed, write input to disk.
        if (fuzzer->emu->exit_reason != EMU_EXIT_REASON_GRACEFUL) {
            if (fuzzer->emu->exit_reason == EMU_EXIT_REASON_SYSCALL_NOT_SUPPORTED) {
                ginger_log(ERROR, "Unsupported syscall!\n");
                abort();
            }
            fuzzer->write_crash(fuzzer);
        }

        // If the fuzz case generated new code coverage, save it to the corpus.
        if (fuzzer->emu->new_coverage) {
            corpus_add_input(fuzzer->emu->corpus, fuzzer->curr_input);
            emu_stats_inc(fuzzer->stats, EMU_COUNTERS_INPUTS);
        }

        // Restore the emulator to its initial state.
        fuzzer->emu->reset(fuzzer->emu, fuzzer->clean_snapshot);

        // Increment the counter counting emulator resets.
        emu_stats_inc(fuzzer->stats, EMU_COUNTERS_RESETS);

        // Update the main stats with data from the thread local stats if the time is right.
        struct timespec current;
        clock_gettime(CLOCK_MONOTONIC, &current);
        const time_t   elapsed_s = current.tv_sec - checkpoint.tv_sec;
        const uint64_t elapsed_ns = (elapsed_s * 1e9) + (current.tv_nsec - checkpoint.tv_nsec);

        // Report stats to the main thread.
        if (elapsed_ns > report_stats_interval_ns) {
            pthread_mutex_lock(&shared_stats->lock);
            shared_stats->nb_executed_instructions += fuzzer->stats->nb_executed_instructions;
            shared_stats->nb_unsupported_syscalls  += fuzzer->stats->nb_unsupported_syscalls;
            shared_stats->nb_fstat_bad_fds         += fuzzer->stats->nb_fstat_bad_fds;
            shared_stats->nb_graceful_exits        += fuzzer->stats->nb_graceful_exits;
            shared_stats->nb_unknown_exit_reasons  += fuzzer->stats->nb_unsupported_syscalls;
            shared_stats->nb_resets                += fuzzer->stats->nb_resets;
            shared_stats->nb_inputs                += fuzzer->stats->nb_inputs;
            shared_stats->nb_segfault_reads        += fuzzer->stats->nb_segfault_reads;
            shared_stats->nb_segfault_writes       += fuzzer->stats->nb_segfault_writes;
            shared_stats->nb_invalid_opcodes       += fuzzer->stats->nb_invalid_opcodes;
            pthread_mutex_unlock(&shared_stats->lock);
            // Reset the timer checkpoint.
            clock_gettime(CLOCK_MONOTONIC, &checkpoint);

            // Clear the local stats.
            memset(fuzzer->stats, 0, sizeof(emu_stats_t));
        }
    }
}

int
main(int argc, char** argv)
{
    // The cpu which the main thread will run on.
    const int main_cpu = 0;
    // Print stats every second.
    const uint64_t print_stats_interval_ns = 1e9;
    // We do not want feed argv[0] of gingersnap to the target.
    const int target_argc = argc - 1;
    // For checking that initialization of the fuzzer is ok.
    int ok = -1;
    // Path to directory where crashes are stored.
    const char* crash_dir = "./data/crashes";
    // Init rng.
    srand(time(NULL));

    // Create shared corpus.
    corpus_t* shared_corpus = corpus_create("./data/corpus/target6");

    // Array of arguments to the target executable.
    heap_str_t target_argv[target_argc];
    memset(target_argv, 0, sizeof(target_argv));
    for (int i = 0; i < target_argc; i++) {
        heap_str_t arg;
        heap_str_set(&arg, argv[i + 1]);
        target_argv[i] = arg;
    }

    // Multiple emus can use the same target since it is only read from and never written to.
    const target_t* target = target_create(target_argc, target_argv);

    // Create an initial emulator, for taking the initial snapshot. This emulator will not be
    // used to fuzz, but the snapshotted state will be passed to the worker emulators as the
    // pre-fuzzed state which they will be reset to after a fuzz case is ran.
    rv_emu_t* initial_emu = emu_create(EMU_TOTAL_MEM, shared_corpus);
    initial_emu->setup(initial_emu, target);

    // Create a debugging CLI using the initial emulator.
    cli_t* debug_cli = debug_cli_create(initial_emu);

    // Run the CLI. If we get a snapshot from it, use it, otherwise exit the program. The snapshot
    // is simply a pointer to the `initial_emu`.
    debug_cli_result_t* cli_result = debug_cli_run(initial_emu, debug_cli);
    while (!cli_result->snapshot_set     ||
           !cli_result->fuzz_buf_adr_set ||
           !cli_result->fuzz_buf_size_set) {

        printf("\nAll mandatory options not set\n"      \
               "Snapshot set:                     %u\n" \
               "Fuzzing buffer start address set: %u\n" \
               "Fuzzing buffer size set:          %u\n",
               cli_result->snapshot_set,
               cli_result->fuzz_buf_adr_set,
               cli_result->fuzz_buf_size_set);
        cli_result = debug_cli_run(initial_emu, debug_cli);
    }
    // Can be used for all threads.
    pthread_attr_t thread_attr = {0};
    pthread_attr_init(&thread_attr);

    // Per-thread info. Multiple threads should never use the same emulator.
    const uint8_t nb_cpus = 1;//nb_active_cpus();
    ginger_log(INFO, "Number active cpus: %u\n", nb_cpus);
    thread_info_t t_info[nb_cpus];
    memset(t_info, 0, sizeof(t_info));

    // Total statistics. Shared between threads. Guarded by a mutex. Updated
    // reguarly at a set time interval.
    emu_stats_t* shared_stats = emu_stats_create();

    // Set the affinity of the main thread to CPU 0.
    const pid_t main_tid = syscall(__NR_gettid);
    cpu_set_t   main_cpu_mask;
    CPU_ZERO(&main_cpu_mask);
    CPU_SET(main_cpu, &main_cpu_mask);
    ok = sched_setaffinity(main_tid, sizeof(main_cpu_mask), &main_cpu_mask);
    if (ok != 0) {
        ginger_log(ERROR, "Failed to set affinity of main thread 0x%x to cpu %d!\n", main_tid, main_cpu);
        abort();
    }

    // Start one fuzzer per available cpu.
    for (uint8_t i = 0; i < nb_cpus; i++) {
        t_info[i].thread_num     = i;
        t_info[i].target         = target;
        t_info[i].shared_stats   = shared_stats;
        t_info[i].corpus         = shared_corpus;
        t_info[i].clean_snapshot = cli_result->snapshot;
        t_info[i].fuzz_buf_adr   = cli_result->fuzz_buf_adr;
        t_info[i].fuzz_buf_size  = cli_result->fuzz_buf_size;
        t_info[i].crash_dir      = crash_dir;

        ok = pthread_create(&t_info[i].thread_id, &thread_attr, &worker_run, &t_info[i]);
        if (ok != 0) {
            ginger_log(ERROR, "[%s] Failed to spawn thread %u\n", __func__, i);
            abort();
        }
        // Migrate worker threads to dedicated cores. One worker will share core with the
        // main thread.
        cpu_set_t worker_cpu_mask;
        CPU_ZERO(&worker_cpu_mask);
        CPU_SET(i, &worker_cpu_mask);
        ok = pthread_setaffinity_np(t_info[i].thread_id, sizeof(worker_cpu_mask), &worker_cpu_mask);
        if (ok != 0) {
            ginger_log(ERROR, "Failed to set affinity of worker thread 0x%x!\n", t_info[i].thread_id);
            abort();
        }
    }
    // We are done with the thread attributes, might as well destroy them.
    ok = pthread_attr_destroy(&thread_attr);
    if (ok != 0) {
        ginger_log(ERROR, "Failed to destroy pthread attributes!\n");
        abort();
    }

    // Report collected statistics every two seconds.
    struct timespec checkpoint;
    struct timespec current;
    clock_gettime(CLOCK_MONOTONIC, &checkpoint);
    uint64_t prev_nb_exec_inst = 0;
    uint64_t prev_nb_resets    = 0;
    for (;;) {
        clock_gettime(CLOCK_MONOTONIC, &current);
        const uint64_t elapsed_s  = current.tv_sec - checkpoint.tv_sec;
        const uint64_t elapsed_ns = (elapsed_s * 1e9) + (current.tv_nsec - checkpoint.tv_nsec);

        // Report every two seconds to give the worker threads enough time to
        // report atleast once.
        if (elapsed_ns > print_stats_interval_ns) {

            // Calculate average number of executed instructions per second.
            const uint64_t nb_exec_this_round = shared_stats->nb_executed_instructions - prev_nb_exec_inst;
            shared_stats->nb_inst_per_sec = nb_exec_this_round / (elapsed_ns / 1e9);

            // Calculate the average number of emulator resets per second.
            const uint64_t nb_resets_this_round = shared_stats->nb_resets - prev_nb_resets;
            shared_stats->nb_resets_per_sec = nb_resets_this_round / (elapsed_ns / 1e9);

            emu_stats_print(shared_stats);

            // Reset the timer checkpoint.
            clock_gettime(CLOCK_MONOTONIC, &checkpoint);

            // Prepare for next loop iteration.
            shared_stats->nb_inst_per_sec   = 0;
            shared_stats->nb_resets_per_sec = 0;
            prev_nb_exec_inst               = shared_stats->nb_executed_instructions;
            prev_nb_resets                  = shared_stats->nb_resets;
        }
        else {
            // This might be suboptimal if the main thread is running on the
            // same cpu as a worker thread(?).
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
    }
    ginger_log(INFO, "All threads joined. Freeing allocated data!\n");
    corpus_destroy(shared_corpus);
    target_destroy((void*)target);

    return 0;
}
