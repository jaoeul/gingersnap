#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "emu_stats.h"

#include "../shared/logger.h"

// Increment counters.
void
emu_stats_inc(emu_stats_t* stats, const enum_emu_counters_t counter)
{
    switch(counter) {
    case EMU_COUNTERS_EXIT_REASON_SYSCALL_NOT_SUPPORTED:
        ++(*stats).nb_unsupported_syscalls;
        break;
    case EMU_COUNTERS_EXIT_FSTAT_BAD_FD:
        ++(*stats).nb_fstat_bad_fds;
        break;
    case EMU_COUNTERS_EXIT_GRACEFUL:
        ++(*stats).nb_graceful_exits;
        break;
    case EMU_COUNTERS_EXECUTED_INSTRUCTIONS:
        ++(*stats).nb_executed_instructions;
        break;
    }
}

void
emu_stats_print(emu_stats_t* stats)
{
    printf("Nb exec inst: %lu", stats->nb_executed_instructions);
    printf(" | Nb non-sup syscall: %lu", stats->nb_unsupported_syscalls);
    printf(" | Nb bad fstat syscalls: %lu", stats->nb_fstat_bad_fds);
    printf(" | Nb graceful exits: %lu", stats->nb_graceful_exits);
    printf(" | Nb unknown exits: %lu\n", stats->nb_unknown_exit_reasons);
}

emu_stats_t*
emu_stats_create(void)
{
    emu_stats_t* stats = calloc(1, sizeof(emu_stats_t));
    if (pthread_mutex_init(&stats->lock, NULL) != 0) {
        ginger_log(ERROR, "Failed to init stats mutex!\n");
        abort();
    }
    return stats;
}

void
emu_stats_destroy(emu_stats_t* stats)
{
    free(stats);
}
