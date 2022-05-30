#ifndef SNAPSHOT_ENGINE_H
#define SNAPSHOT_ENGINE_H

#include <stdbool.h>
#include <stdint.h>

#include "../corpus/corpus.h"
#include "../emu/emu_generic.h"

typedef struct snapshot_engine snapshot_engine_t;

struct snapshot_engine {
    emu_t*          emu;               // Emulator used by the engine.
    emu_stats_t*    stats;             // Engine local stats.
    const emu_t*    clean_snapshot;    // Pre-fuzzed emulator state.
    uint64_t        fuzz_buf_adr;      // Guest address to inject fuzzcases at, assuming that the emulator state is the clean snapshot.
    uint64_t        fuzz_buf_size;     // Size of the buffer to fuzz.
    pid_t           tid;               // ID of thread which runs the engine.
    input_t*        curr_input;        // The input data of the current fuzzcase.
    const char*     crash_dir;         // The path to the directory where inputs which caused crashes are stored.

    // Pick a random input from the corpus, mutate it, inject int into emulator memory
    // and run the emulator.
    enum_emu_exit_reasons_t (*fuzz)(snapshot_engine_t* engine);

    // Mutate random amount of bytes in a buffer, up to the total number of bytes in buffer.
    void (*mutate)(uint8_t* input, const uint64_t len);

    // Inject a fuzzcase into emulator memory.
    void (*inject)(snapshot_engine_t* snap, const uint8_t* input, const uint64_t len);

    // Write input which caused a crash to disk. Assumes that the fuzzcase
    // which is currently loaded is the one which caused the crash.
    void (*write_crash)(snapshot_engine_t* snap);
};

snapshot_engine_t*
snapshot_engine_create(enum_supported_archs_t arch, corpus_t* corpus, uint64_t fuzz_buf_adr, uint64_t fuzz_buf_size,
              const target_t* target, const emu_t* snapshot, const char* crash_dir);

void
snapshot_engine_destroy(snapshot_engine_t* snap);

#endif
