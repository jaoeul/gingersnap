#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <time.h>
#include <unistd.h>

#include "fuzzer.h"

#include "../emu/riscv_emu.h"
#include "../shared/logger.h"

// Change randomize random amount of bytes in a buffer, up to the total number
// of bytes in buffer.
static void
fuzzer_mutate(uint8_t* input, const uint64_t len)
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
fuzzer_inject(fuzzer_t* fuzzer, const uint8_t* input, const uint64_t len)
{
    // Save current permissions of the target buffer.
    uint8_t tmp_perms[len];
    memcpy(tmp_perms, fuzzer->emu->mmu->permissions + fuzzer->fuzz_buf_adr, len);

    // Change permissions of the target buffer to writeable.
    fuzzer->emu->mmu->set_permissions(fuzzer->emu->mmu, fuzzer->fuzz_buf_adr, PERM_WRITE, len);

    // Write the input into the target buffer.
    fuzzer->emu->mmu->write(fuzzer->emu->mmu, fuzzer->fuzz_buf_adr, input, len);

    // Change the permissions of the target buffer back.
    memcpy(fuzzer->emu->mmu->permissions + fuzzer->fuzz_buf_adr, tmp_perms, len);
}

static enum_emu_exit_reasons_t
fuzzer_fuzz(fuzzer_t* fuzzer)
{
    // Can be removed for perf.
    const pid_t actual_tid = syscall(__NR_gettid);
    if (fuzzer->tid != actual_tid) {
        ginger_log(ERROR, "[%s] Thread tried to execute someone elses emu!\n", __func__);
        ginger_log(ERROR, "[%s] acutal_tid 0x%x, emu->tid: 0x%x\n", __func__, actual_tid, fuzzer->tid);
        abort();
    }

    if (fuzzer->emu->corpus->inputs->length == 0) {
        ginger_log(ERROR, "Abort! Empty corpus!\n");
        abort();
    }

    // Pick a random input from the shared corpus.
    // TODO: Make atomic?
    const int r            = rand() % fuzzer->emu->corpus->inputs->length;
    input_t*  chosen_input = vector_get(fuzzer->emu->corpus->inputs, r);

    // If the input length is less than the fuzzers buffer length, use that instead, as there
    // is no reason to memcpy a bunch of zeroes.
    uint64_t effective_len = 0;
    if (chosen_input->length < fuzzer->fuzz_buf_size) {
        effective_len = chosen_input->length;
    }
    else {
        effective_len = fuzzer->fuzz_buf_size;
    }
    fuzzer->curr_input->length = effective_len;

    // Copy the data from the corpus to a fuzzer owned buffer, which we will mutate.
    // If the mutation crashes the emulator after injecton, we write this input
    // disk.
    fuzzer->curr_input->data = realloc(fuzzer->curr_input->data, fuzzer->curr_input->length);
    if (!fuzzer->curr_input->data) {
        ginger_log(ERROR, "[%s] Could not reallocate buffer for input data! Requested size: %lu\n",
                   fuzzer->curr_input->length, __func__);
        abort();
    }
    memcpy(fuzzer->curr_input->data, chosen_input->data, fuzzer->curr_input->length);

    // Mutate the input.
    fuzzer->mutate(fuzzer->curr_input->data, fuzzer->curr_input->length);

    // Inject the input.
    fuzzer->inject(fuzzer, fuzzer->curr_input->data, fuzzer->curr_input->length);

    // Run the emulator until it exits or crashes.
    return fuzzer->emu->run(fuzzer->emu, fuzzer->stats);
}

static void
fuzzer_write_crash(fuzzer_t* fuzzer)
{
    char filepath[4096] = {0};
    char filename[255]  = {0};
    char timestamp[21]  = {0};

    // Base filename on crash type and system time.
    switch(fuzzer->emu->exit_reason) {
        case EMU_EXIT_REASON_SEGFAULT_READ:
            memcpy(filename, "segfault-read-", 14);
            break;
        case EMU_EXIT_REASON_SEGFAULT_WRITE:
            memcpy(filename, "segfault-write-", 15);
            break;
        default:
            return;
    }
    struct timespec spec;

    // Year, month, day, hour, minute, second.
    clock_gettime(CLOCK_REALTIME, &spec);
    time_t curr_time = spec.tv_sec;
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
    strcat(filepath, fuzzer->crash_dir);
    strcat(filepath, "/");
    strcat(filepath, filename);

    // Write crash to file.
    FILE* fp = fopen(filepath, "wb");
    if (!fp) {
        ginger_log(ERROR, "Failed to open crash file for writing!\n");
    }
    if (!fwrite(fuzzer->curr_input->data, 1, fuzzer->curr_input->length, fp)) {
        ginger_log(ERROR, "Failed to write to crash file!\n");
    }
    fclose(fp);
}

fuzzer_t*
fuzzer_create(corpus_t* corpus, uint64_t fuzz_buf_adr, uint64_t fuzz_buf_size, const target_t* target,
              const rv_emu_t* snapshot, const char* crash_dir)
{
    fuzzer_t* fuzzer = calloc(1, sizeof(fuzzer_t));

    // Create and setup the emulator this fuzzer will use.
    rv_emu_t* emu = emu_create(EMU_TOTAL_MEM, corpus);

    // Init the emulator thread id.
    // Load the elf and build the stack.
    emu->setup(emu, target);

    fuzzer->tid               = syscall(__NR_gettid);
    fuzzer->emu               = emu;
    fuzzer->fuzz_buf_adr      = fuzz_buf_adr;
    fuzzer->fuzz_buf_size     = fuzz_buf_size;
    fuzzer->crash_dir         = crash_dir;
    fuzzer->clean_snapshot    = snapshot;
    fuzzer->curr_input        = corpus_input_create();
    fuzzer->stats             = emu_stats_create();
    fuzzer->stats->nb_inputs  = corpus->inputs->length;

    // API
    fuzzer->fuzz              = fuzzer_fuzz;
    fuzzer->mutate            = fuzzer_mutate;
    fuzzer->inject            = fuzzer_inject;
    fuzzer->write_crash       = fuzzer_write_crash;

    return fuzzer;
}

void
fuzzer_destroy(fuzzer_t* fuzzer)
{
    fuzzer->emu->destroy(fuzzer->emu);
    emu_stats_destroy(fuzzer->stats);
    free(fuzzer);
}
