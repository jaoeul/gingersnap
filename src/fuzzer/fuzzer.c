#include <syscall.h>
#include <unistd.h>

#include "fuzzer.h"

#include "../emu/riscv_emu.h"
#include "../shared/logger.h"

void
fuzzer_fuzz_input(fuzzer_t* fuzzer)
{
    // Can be removed for perf.
    const pid_t actual_tid = syscall(__NR_gettid);
    if (fuzzer->tid != actual_tid) {
        ginger_log(ERROR, "[%s] Thread tried to execute someone elses emu!\n", __func__);
        ginger_log(ERROR, "[%s] acutal_tid 0x%x, emu->tid: 0x%x\n", __func__, actual_tid, fuzzer->tid);
        abort();
    }

    // Pick a random input from the corpus.

    // Mutate the input.

    // Inject the input.

    // Run the emulator until it exits or crashes.
    fuzzer->emu->run(fuzzer->emu, fuzzer->stats);

    // Restore the emulator to its initial state.
    fuzzer->emu->reset(fuzzer->emu, fuzzer->clean_snapshot);
    emu_stats_inc(fuzzer->stats, EMU_COUNTERS_RESETS);
}

fuzzer_t*
fuzzer_create(corpus_t* corpus, uint64_t fuzz_buf_adr, uint64_t fuzz_buf_size, const target_t* target, const rv_emu_t* snapshot)
{
    fuzzer_t* fuzzer = calloc(1, sizeof(fuzzer_t));

    // Create and setup the emulator this fuzzer will use.
    rv_emu_t* emu = emu_create(EMU_TOTAL_MEM);
    // Init the emulator thread id.
    // Load the elf and build the stack.
    emu->setup(emu, target);

    fuzzer->tid           = syscall(__NR_gettid);
    fuzzer->emu           = emu;
    fuzzer->corpus        = corpus;
    fuzzer->fuzz_buf_adr  = fuzz_buf_adr;
    fuzzer->fuzz_buf_size = fuzz_buf_size;

    // Create the thread local stats structure.
    fuzzer->stats = emu_stats_create();

    // Capture the pre fuzzed state.
    fuzzer->clean_snapshot = snapshot;

    // API
    fuzzer->fuzz_input = fuzzer_fuzz_input;

    return fuzzer;
}

void
fuzzer_destroy(fuzzer_t* fuzzer);
