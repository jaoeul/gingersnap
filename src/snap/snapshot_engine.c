#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <time.h>
#include <unistd.h>

#include "snapshot_engine.h"

#include "../emu/emu_generic.h"
#include "../utils/dir.h"
#include "../utils/logger.h"

// Change randomize random amount of bytes in a buffer, up to the total number
// of bytes in buffer.
static void
snapshot_engine_mutate(uint8_t* input, const uint64_t len)
{
    const int nb_mut = rand() % len; // The random number of bytes to mutate.
    // Mutate atleast one byte. Max amount is equal to the size of the buffer.
    for (int i = 0; i < nb_mut + 1; i++) {
        // Pick a random byte to mutate.
        const uint64_t rand_idx = rand() % len;

        // Mutate the chosen index to a random byte.
        // TODO: 0xff + 1, to enable setting the rand_byte to 0xff?
        const uint8_t rand_byte = rand() % 0xff;

        // Set the random byte to the random value.
        input[rand_idx] = rand_byte;
    }
}

// Inject a fuzzcase into emulator memory.
static void
snapshot_engine_inject(snapshot_engine_t* engine, const uint8_t* input, const uint64_t len)
{
    mmu_t* mmu = engine->emu->get_mmu(engine->emu);

    // Save current permissions of the target buffer.
    uint8_t tmp_perms[len];
    memcpy(tmp_perms, mmu->permissions + engine->fuzz_buf_adr, len);

    // Change permissions of the target buffer to writeable.
    mmu->set_permissions(mmu, engine->fuzz_buf_adr, MMU_PERM_WRITE, len);

    // Write the input into the target buffer.
    mmu->write(mmu, engine->fuzz_buf_adr, input, len);

    // Change the permissions of the target buffer back.
    memcpy(mmu->permissions + engine->fuzz_buf_adr, tmp_perms, len);
}

static enum_emu_exit_reasons_t
snapshot_engine_fuzz(snapshot_engine_t* engine)
{
    const pid_t actual_tid = syscall(__NR_gettid);
    corpus_t* corpus = engine->emu->get_corpus(engine->emu);

    if (engine->tid != actual_tid) {
        ginger_log(ERROR, "[%s] Thread tried to execute someone elses emu!\n", __func__);
        ginger_log(ERROR, "[%s] acutal_tid 0x%x, emu->tid: 0x%x\n", __func__, actual_tid, engine->tid);
        abort();
    }

    if (corpus->inputs->length == 0) {
        ginger_log(ERROR, "Abort! Empty corpus!\n");
        abort();
    }

    // Pick a random input from the shared corpus.
    // TODO: Make atomic?
    const int r                 = rand() % corpus->inputs->length;
    const input_t* chosen_input = vector_get(corpus->inputs, r);
    if (!chosen_input) {
        ginger_log(ERROR, "Abort! Failed to pick an input from the corpus!\n");
        abort();
    }

    // If the input length is less than the snapshot_engines buffer length, use that instead, as there
    // is no reason to memcpy a bunch of zeroes.
    uint64_t effective_len = 0;
    if (chosen_input->length < engine->fuzz_buf_size) {
        effective_len = chosen_input->length;
    }
    else {
        effective_len = engine->fuzz_buf_size;
    }
    if (effective_len == 0) {
        ginger_log(ERROR, "Abort! Fuzz case length is 0!\n");
        abort();
    }

    // Copy the data from the corpus to a snapshot_engine owned buffer, which we will mutate.
    // If the mutation crashes the emulator after injecton, we write this input
    // to disk.
    engine->curr_input = corpus_input_copy(chosen_input);
    if (!engine->curr_input) {
        ginger_log(ERROR, "[%s] Could not reallocate buffer for input data! Requested size: %lu\n",
                   engine->curr_input->length, __func__);
        abort();
    }

    // Mutate the input.
    engine->mutate(engine->curr_input->data, engine->curr_input->length);

    // Inject the input.
    engine->inject(engine, engine->curr_input->data, engine->curr_input->length);

    // Run the emulator until it exits or crashes.
    return engine->emu->run(engine->emu, engine->stats);
}

static void
snapshot_engine_write_crash(snapshot_engine_t* engine)
{
    char filepath[4096] = {0};
    char filename[255]  = {0};
    char timestamp[21]  = {0};

    // Base filename on crash type and system time.
    enum_emu_exit_reasons_t exit_reason = engine->emu->get_exit_reason(engine->emu);
    switch (exit_reason)
    {
    case EMU_EXIT_REASON_SEGFAULT_READ:
        memcpy(filename, "segfault-read-", 14);
        break;
    case EMU_EXIT_REASON_SEGFAULT_WRITE:
        memcpy(filename, "segfault-write-", 15);
        break;
    default:
        return;
    }

    // Year, month, day, hour, minute, second.
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    time_t curr_time    = spec.tv_sec;
    struct tm* timeinfo = localtime(&curr_time);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d-%H:%M:%S:", timeinfo);
    strcat(filename, timestamp);

    // Nano seconds.
    char nanoseconds[10] = {0};
    sprintf(nanoseconds, "%lu", spec.tv_nsec);
    strcat(filename, nanoseconds);

    // Extension.
    strcat(filename, ".crash");

    // Build filepath.
    strcat(filepath, engine->crash_dir);
    strcat(filepath, "/");
    strcat(filepath, filename);

    // Write crash to file.
    FILE* fp = fopen(filepath, "wb");
    if (!fp) {
        ginger_log(ERROR, "Failed to open crash file for writing!\n");
    }
    if (!fwrite(engine->curr_input->data, 1, engine->curr_input->length, fp)) {
        ginger_log(ERROR, "Failed to write to crash file!\n");
    }
    fclose(fp);
}

snapshot_engine_t*
snapshot_engine_create(enum_supported_archs_t arch, corpus_t* corpus, uint64_t fuzz_buf_adr, uint64_t fuzz_buf_size,
              const target_t* target, const emu_t* snapshot, const char* crash_dir)
{
    snapshot_engine_t* engine = calloc(1, sizeof(snapshot_engine_t));

    // Create and setup the emulator this snapshot_engine will use.
    emu_t* emu = emu_create(arch, EMU_TOTAL_MEM, corpus);

    emu->load_elf(emu, target);
    emu->build_stack(emu, target);

    engine->tid               = syscall(__NR_gettid);
    engine->emu               = emu;
    engine->fuzz_buf_adr      = fuzz_buf_adr;
    engine->fuzz_buf_size     = fuzz_buf_size;
    engine->crash_dir         = crash_dir;
    engine->clean_snapshot    = snapshot;
    engine->stats             = emu_stats_create();

    // API
    engine->fuzz              = snapshot_engine_fuzz;
    engine->mutate            = snapshot_engine_mutate;
    engine->inject            = snapshot_engine_inject;
    engine->write_crash       = snapshot_engine_write_crash;

    return engine;
}

void
snapshot_engine_destroy(snapshot_engine_t* engine)
{
    emu_destroy(engine->emu);
    emu_stats_destroy(engine->stats);
    free(engine);
}
