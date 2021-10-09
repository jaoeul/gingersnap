#ifndef FUZZER_H
#define FUZZER_H

#include <stdint.h>

#include "../corpus/corpus.h"
#include "../emu/riscv_emu.h"

typedef struct fuzzer fuzzer_t;

struct fuzzer {
    corpus_t*       corpus;            // Data that the injected input is based on. Shared between all fuzzers.
    rv_emu_t*       emu;               // Emulator used by the fuzzer.
    emu_stats_t*    stats;             // Fuzzer local stats.
    const rv_emu_t* clean_snapshot;    // Pre-fuzzed emulator state.
    uint64_t        fuzz_buf_adr;      // Guest address to inject fuzzcases at, assuming that the emulator state is the clean snapshot.
    uint64_t        fuzz_buf_size;     // Size of the buffer to fuzz.
    pid_t           tid;               // ID of thread which runs the fuzzer.
    uint8_t*        curr_fuzzcase;     // The input data of the current fuzzcase.
    uint64_t        curr_fuzzcase_len; // The length of the input data of the current fuzzcase
    const char*     crash_dir;         // The path to the directory where inputs which caused crashes are stored.

    // Pick a random input from the corpus, mutate it, inject int into emulator memory
    // and run the emulator.
    enum_emu_exit_reasons_t (*fuzz)(fuzzer_t* fuzzer);

    // Mutate random amount of bytes in a buffer, up to the total number of bytes in buffer.
    void (*mutate)(uint8_t* input, const uint64_t len);

    // Inject a fuzzcase into emulator memory.
    void (*inject)(fuzzer_t* fuzzer, const uint8_t* input, const uint64_t len);

    // Write input which caused a crash to disk. Assumes that the fuzzcase
    // which is currently loaded is the one which caused the crash.
    void (*write_crash)(fuzzer_t* fuzzer);
};

// TODO: Should take the const char* to the corpus dir instead of a corpus struct.
fuzzer_t*
fuzzer_create(corpus_t* corpus, uint64_t fuzz_buf_adr, uint64_t fuzz_buf_size, const target_t* target,
              const rv_emu_t* snapshot, const char* crash_dir);

void
fuzzer_destroy(fuzzer_t* fuzzer);

#endif
