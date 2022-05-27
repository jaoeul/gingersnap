#define _GNU_SOURCE

#include <getopt.h>
#include <malloc.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "sig_handler.h"

#include "../elf_loader/elf_loader.h"
#include "../emu/emu_generic.h"
#include "../emu/emu_stats.h"
#include "../fuzzer/fuzzer.h"
#include "../debug_cli/debug_cli.h"
#include "../utils/cli.h"
#include "../utils/dir.h"
#include "../utils/endianess.h"
#include "../utils/hstring.h"
#include "../utils/logger.h"
#include "../utils/print_utils.h"
#include "../utils/vector.h"
#include "../target/target.h"

#define cpu_relax() asm volatile("rep; nop")

static const char usage_string[] = ""
"Usage:\n"
"gingersnap -t \"<target> <arg_1> ... <arg_n>\" -c <corpus_dir> -a <arch>\n"
" -t, --target        Target program and arguments.\n"
" -c, --corpus        Path to directory with corpus files.\n"
" -a, --arch          Architecture to emulate.\n"
" -j, --jobs          Number of cores to use for fuzzing. Defauts to all active cores on the\n"
"                     system.\n"
" -p, --progress      Progress directory, where inputs which generated new coverage will be\n"
"                     stored. Defaults to `./progress`.\n"
" -v, --verbose       Print stdout from emulators to stdout.\n"
" -n, --no-coverage   No coverage. Do not track coverage.\n"
" -h, --help          Print this help text.\n\n"
"Supported architectures:\n"
" - rv64i [RISC V 64 bit]\n\n"
"Available pre-fuzzing commands:\n"
" xmem       Examine emulator memory.\n"
" smem       Search for sequence of bytes in guest memory.\n"
" ni         Execute next instruction.\n"
" ir         Show emulator registers.\n"
" break      Set breakpoint.\n"
" watch      Set register watchpoint.\n"
" sbreak     Show all breakpoints.\n"
" swatch     Show all watchpoints.\n"
" continue   Run emulator until breakpoint or program exit.\n"
" snapshot   Take a snapshot.\n"
" adr        Set the address in guest memory where fuzzed input will be injected.\n"
" length     Set the fuzzer injection input length.\n"
" go         Try to start the fuzzer.\n"
" options    Show values of the adjustable options.\n"
" help       Displays help text of a command.\n"
" quit       Quit debugging and exit this program.\n\n"
"Run `help <command>` in gingersnap for further details and examples of command\n"
"usage.\n\n"
"Typical usage example:\n"
"Step 1: Run the emulator to desireable pre-fuzzing state. This can be done by\n"
"        single-stepping or by setting a breakpoint and continuing exection.\n"
"(gingersnap) ni\n"
"(gingersnap) break <guest_address>\n"
"(gingersnap) continue\n\n"
"Step 2: Set the address and length of the buffer in guest memory where\n"
"        fuzzcases will be injected. This is a required step.\n"
"(gingersnap) adr <guest_address>\n"
"(gingersnap) len <length>\n\n"
"Step 3: Start fuzzing:\n"
"(gingersnap) go\n";

// Declared in `config.h`.
extern global_config_t global_config;

// Used as argument to the threads running the emulators.
typedef struct {
    pthread_t       thread_id;    // ID returned by pthread_create().
    uint64_t        thread_num;   // Application-defined thread number.
    const target_t* target;       // The target executable.
    emu_stats_t*    shared_stats; // Collected statistics, reported by the worker threads.
    corpus_t*       corpus;       // Data which the fuzz inputs are based on. Shared between threads.
    uint64_t        fuzz_buf_adr;
    uint64_t        fuzz_buf_size;
    const emu_t*    clean_snapshot;
} thread_info_t;

static char*
arch_to_str(enum_supported_archs_t arch)
{
    switch (arch)
    {
        case ENUM_SUPPORTED_ARCHS_RISCV64I_LSB:
            return "RISCV64i LSB";
        case ENUM_SUPPORTED_ARCHS_MIPS64_MSB:
            return "MIPS64 MSB";
        default:
            return "Unrecognized";
    }
}

static void
usage_string_print(void)
{
    printf("%s", usage_string);
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
    const emu_t*    clean_snapshot = t_info->clean_snapshot;
    emu_stats_t*    shared_stats   = t_info->shared_stats;

    // Create the thread local fuzzer.
    fuzzer_t* fuzzer = fuzzer_create(global_config_get_arch(),
                                    corpus,
                                    fuzz_buf_adr,
                                    fuzz_buf_size,
                                    target,
                                    clean_snapshot,
                                    global_config_get_crashes_dir());

    // A timestamp which is used for comparison.
    struct timespec checkpoint;
    clock_gettime(CLOCK_MONOTONIC, &checkpoint);

    for (;;) {
        // Run one fuzzcase.
        fuzzer->fuzz(fuzzer);

        // If we crashed, write input to disk.
        enum_emu_exit_reasons_t exit_reason = fuzzer->emu->get_exit_reason(fuzzer->emu);
        if (exit_reason != EMU_EXIT_REASON_GRACEFUL) {
            if (exit_reason == EMU_EXIT_REASON_SYSCALL_NOT_SUPPORTED) {
                ginger_log(ERROR, "Unsupported syscall!\n");
                abort();
            }
            fuzzer->write_crash(fuzzer);
        }

        // If the fuzz case generated new code coverage, save it to the corpus.
        if (fuzzer->emu->get_new_coverage(fuzzer->emu)) {
            corpus_add_input(fuzzer->emu->get_corpus(fuzzer->emu), fuzzer->curr_input);
            emu_stats_inc(fuzzer->stats, EMU_COUNTERS_INPUTS);
        }
        // We do not care for inputs which did not generate new coverage, so we
        // can free it.
        else {
            corpus_input_destroy(fuzzer->curr_input);
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

static void
handle_cli_args(int argc, char** argv)
{
    bool ok = true;
    static struct option long_options[] = {
        {"target",       required_argument, NULL, 't'},
        {"corpus",       required_argument, NULL, 'c'},
        {"arch",         required_argument, NULL, 'a'},
        {"jobs",         required_argument, NULL, 'j'},
        {"progress",     required_argument, NULL, 'p'},
        {"verbosity",    no_argument,       NULL, 'v'},
        {"no-coverage",  no_argument,       NULL, 'n'},
        {"help",         no_argument,       NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    int ch = -1;
    while ((ch = getopt_long(argc, argv, "t:c:j:p:a:vnh", long_options, NULL)) != -1) {
        switch (ch)
        {
        case 't':
            global_config_set_target(optarg);
            break;
        case 'c':
            global_config_set_corpus_dir(optarg);
            break;
        case 'a':
            global_config_set_arch(optarg);
            break;
        case 'j':
            global_config_set_nb_cpus(strtoul(optarg, NULL, 10));
            break;
        case 'p':
            global_config_set_progress_dir(optarg);
            break;
        case 'v':
            global_config_set_verbosity(true);
            break;
        case 'n':
            global_config_set_coverage(false);
            break;
        case 'h':
            usage_string_print();
            exit(0);
        default:
            exit(1);
        }
    }

    if (!global_config_get_target()) {
        ginger_log(ERROR, "Missing required argument [-t, --target]\n");
        ok = false;
    }
    if (!global_config_get_corpus_dir()) {
        ginger_log(ERROR, "Missing required argument [-c, --corpus]\n");
        ok = false;
    }
    if (global_config_get_arch() == ENUM_SUPPORTED_ARCHS_INVALID) {
        ginger_log(ERROR, "Invalid or missing required argument [-a, --arch]\n");
        ok = false;
    }

    if (!ok) {
        exit(1);
    }

    ginger_log(INFO, "Jobs:         %lu\n", global_config_get_nb_cpus());
    ginger_log(INFO, "Verbosity:    %s\n",  global_config_get_verbosity() ? "true" : "false");
    ginger_log(INFO, "Coverage:     %s\n",  global_config_get_coverage() ? "true" : "false");
    ginger_log(INFO, "Corpus dir:   %s\n",  global_config_get_corpus_dir());
    ginger_log(INFO, "Target:       %s\n",  global_config_get_target());
    ginger_log(INFO, "Progress dir: %s\n",  global_config_get_progress_dir());
    ginger_log(INFO, "Arch:         %s\n",  arch_to_str(global_config_get_arch()));
}

static uint8_t
nb_active_cpus(void)
{
    uint8_t nb_cpus = 0;
    char*   line = NULL;
    size_t  len  = 0;
    ssize_t read = 0;

    FILE* fp = fopen("/proc/cpuinfo", "rb");
    if (!fp) {
        printf("Could not open /proc/cpuinfo\n");
        abort();
    }
    while ((read = getline(&line, &len, fp)) != -1) {
        if (strstr(line, "processor")) {
            nb_cpus++;
        }
    }
    return nb_cpus;
}

static void
init_default_config(void)
{
    global_config_set_coverage(true);
    global_config_set_nb_cpus(nb_active_cpus());
    global_config_set_progress_dir("./progress");
}

static bool
output_dirs_create(void)
{
    char*       crash_dir    = calloc(1024, 1);
    const char* progress_dir = global_config_get_progress_dir();

    strcpy(crash_dir, progress_dir);
    strcat(crash_dir, "/crashes");
    if (!create_dir_ifn_exist(progress_dir)) {
        ginger_log(ERROR, "Failed to create %s dir!\n", progress_dir);
        exit(1);
    }

    if (!create_dir_ifn_exist(crash_dir)) {
        ginger_log(ERROR, "Failed to create %s dir!\n", crash_dir);
        exit(1);
    }
    global_config_set_crashes_dir(crash_dir);
    ginger_log(INFO, "Crashes dir: %s\n", global_config_get_crashes_dir());

    if (global_config_get_coverage()) {
        char* inputs_dir = calloc(1024, 1);
        strcpy(inputs_dir, progress_dir);
        strcat(inputs_dir, "/inputs");
        if (!create_dir_ifn_exist(inputs_dir)) {
            ginger_log(ERROR, "Failed to create %s dir!\n", inputs_dir);
            exit(1);
        }
        global_config_set_inputs_dir(inputs_dir);
        ginger_log(INFO, "Inputs dir: %s\n", global_config_get_inputs_dir());
    }
    return true;
}

static void
output_dirs_destroy(void)
{
    char* crash_dir  = global_config_get_crashes_dir();
    free(crash_dir);

    if (global_config_get_coverage()) {
        char* inputs_dir = global_config_get_inputs_dir();
        free(inputs_dir);
    }
}

int
main(int argc, char** argv)
{
    const int      main_cpu                = 0;   // The cpu which the main thread will run on.
    const uint64_t print_stats_interval_ns = 1e9; // Print stats every second.
    int            ok                      = -1;  // For checking initialization steps.

    init_sig_handler();
    init_default_config();

    // Provided cli args overwrites the default config.
    handle_cli_args(argc, argv);

    srand(time(NULL));

    if (!output_dirs_create()) {
        ginger_log(ERROR, "Failed to create output dirs!\n");
        exit(1);
    }

    // Array of arguments to the target executable.
    token_str_t* target_tokens = token_str_tokenize(global_config_get_target(), " ");
    hstring_t    target_argv[target_tokens->nb_tokens];
    memset(target_argv, 0, sizeof(target_argv));
    for (int i = 0; i < target_tokens->nb_tokens; i++) {
        hstring_t arg;
        hstring_set(&arg, target_tokens->tokens[i]);
        target_argv[i] = arg;
    }

    // Multiple emus can use the same target since it is only read from and never written to.
    const target_t* target = target_create(target_tokens->nb_tokens, target_argv);

    // Create an initial emulator, for taking the initial snapshot. This emulator will not be
    // used to fuzz, but the snapshotted state will be passed to the worker emulators as the
    // pre-fuzzed state which they will be reset to after a fuzz case is ran.
    corpus_t* shared_corpus = corpus_create(global_config_get_corpus_dir());

    emu_t* initial_emu = emu_create(global_config_get_arch(), EMU_TOTAL_MEM, shared_corpus);

    initial_emu->load_elf(initial_emu, target);
    initial_emu->build_stack(initial_emu, target);

    // Create a debugging CLI using the initial emulator.
    cli_t* debug_cli = debug_cli_create();

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
    const uint64_t nb_cpus = global_config_get_nb_cpus();
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

            // Get the number of total inputs currently in the corpus.
            emu_stats_print(shared_stats);
            shared_stats->nb_inputs = shared_corpus->inputs->length;

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
    output_dirs_destroy();

    return 0;
}
