#ifndef EMU_STATS_H
#define EMU_STATS_H

#include <pthread.h>
#include <stdint.h>

typedef enum {
    EMU_COUNTERS_EXIT_REASON_SYSCALL_NOT_SUPPORTED,
    EMU_COUNTERS_EXIT_FSTAT_BAD_FD,
    EMU_COUNTERS_EXIT_SEGFAULT_READ,
    EMU_COUNTERS_EXIT_SEGFAULT_WRITE,
    EMU_COUNTERS_EXIT_INVALID_OPCODE,
    EMU_COUNTERS_EXIT_GRACEFUL,
    EMU_COUNTERS_EXECUTED_INSTRUCTIONS,
    EMU_COUNTERS_RESETS,
    EMU_COUNTERS_INPUTS,
} enum_emu_counters_t;

typedef enum {
    EMU_EXIT_REASON_NO_EXIT = 0,
    EMU_EXIT_REASON_SYSCALL_NOT_SUPPORTED,
    EMU_EXIT_REASON_FSTAT_BAD_FD,
    EMU_EXIT_REASON_SEGFAULT_READ,
    EMU_EXIT_REASON_SEGFAULT_WRITE,
    EMU_EXIT_REASON_INVALID_OPCODE,
    EMU_EXIT_REASON_GRACEFUL,
} enum_emu_exit_reasons_t;

typedef struct {
    uint64_t nb_executed_instructions;
    uint64_t nb_unsupported_syscalls;
    uint64_t nb_fstat_bad_fds;
    uint64_t nb_segfault_reads;
    uint64_t nb_segfault_writes;
    uint64_t nb_invalid_opcodes;
    uint64_t nb_graceful_exits;
    uint64_t nb_unknown_exit_reasons;
    uint64_t nb_resets;
    uint64_t nb_inputs;
    double   nb_inst_per_sec;
    double   nb_resets_per_sec;

    // Lock for synchronizing updating stats from multiple workers to the main
    // stats structure.
    pthread_mutex_t lock;
} emu_stats_t;

void emu_stats_inc(emu_stats_t* stats, const enum_emu_counters_t counter);

void emu_stats_report_exit_reason(emu_stats_t* stats, enum_emu_exit_reasons_t exit_reason);

void emu_stats_print(emu_stats_t* stats);

emu_stats_t* emu_stats_create(void);

void emu_stats_destroy(emu_stats_t* stats);

#endif
