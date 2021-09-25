#ifndef FUZZER_H
#define FUZZER_H

#include <stdint.h>

#include "../corpus/corpus.h"
#include "../emu/riscv_emu.h"

typedef struct fuzzer fuzzer_t;

struct fuzzer {
    corpus_t*    corpus;         // Data that the injected input is based on. Shared between all fuzzers.
    rv_emu_t*    emu;            // Emulator used by the fuzzer.
    uint64_t     fuzz_buf_adr;   // Guest address to inject fuzzcases at.
    uint64_t     fuzz_buf_size;  // Size of the buffer to fuzz.
    emu_stats_t* stats;          // Fuzzer local stats.
    rv_emu_t*    clean_snapshot; // Pre-fuzzed emulator state.
    pid_t        tid;            // ID of thread which runs the fuzzer.

    // API
    void (*fuzz_input)(fuzzer_t* fuzzer);

};

fuzzer_t*
fuzzer_create(corpus_t* corpus, uint64_t fuzz_buf_adr, uint64_t fuzz_buf_size, const target_t* target);

void
fuzzer_destroy(fuzzer_t* fuzzer);

#endif
