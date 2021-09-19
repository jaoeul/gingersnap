#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "emu_stats.h"

#include "../shared/logger.h"

// Increment counters.
void
emu_stats_inc(emu_stats_t* stats, const enum_emu_counters_t counter)
{
    switch(counter) {
    case EMU_COUNTERS_EXIT_REASON_SYSCALL_NOT_SUPPORTED:
        ++stats->nb_unsupported_syscalls;
        break;
    case EMU_COUNTERS_EXIT_FSTAT_BAD_FD:
        ++stats->nb_fstat_bad_fds;
        break;
    case EMU_COUNTERS_EXIT_GRACEFUL:
        ++stats->nb_graceful_exits;
        break;
    case EMU_COUNTERS_EXECUTED_INSTRUCTIONS:
        ++stats->nb_executed_instructions;
        break;
    case EMU_COUNTERS_RESETS:
        ++stats->nb_resets;
        break;
    }
}

void
emu_stats_print(emu_stats_t* stats)
{
    char stats_buf[1024] = {0};
    char tmp_buf[256]    = {0};

    sprintf(tmp_buf, "Nb exec inst: %lu", stats->nb_executed_instructions);
    strcat(stats_buf, tmp_buf);
    memset(tmp_buf, 0, sizeof(tmp_buf));

    sprintf(tmp_buf, " | Nb non-sup syscall: %lu", stats->nb_unsupported_syscalls);
    strcat(stats_buf, tmp_buf);
    memset(tmp_buf, 0, sizeof(tmp_buf));

    sprintf(tmp_buf, " | Nb bad fstat syscalls: %lu", stats->nb_fstat_bad_fds);
    strcat(stats_buf, tmp_buf);
    memset(tmp_buf, 0, sizeof(tmp_buf));

    sprintf(tmp_buf, " | Nb graceful exits: %lu", stats->nb_graceful_exits);
    strcat(stats_buf, tmp_buf);
    memset(tmp_buf, 0, sizeof(tmp_buf));

    sprintf(tmp_buf, " | Nb unknown exits: %lu", stats->nb_unknown_exit_reasons);
    strcat(stats_buf, tmp_buf);
    memset(tmp_buf, 0, sizeof(tmp_buf));

    sprintf(tmp_buf, " | Nb resets: %lu", stats->nb_resets);
    strcat(stats_buf, tmp_buf);
    memset(tmp_buf, 0, sizeof(tmp_buf));

    // Instructions per second.
    sprintf(tmp_buf, " | Avg nb inst per sec: %lf", stats->avg_nb_inst_per_sec);
    strcat(stats_buf, tmp_buf);
    memset(tmp_buf, 0, sizeof(tmp_buf));

    // Resets per second.
    sprintf(tmp_buf, " | Avg nb resets per sec: %lf", stats->avg_nb_resets_per_sec);
    strcat(stats_buf, tmp_buf);
    memset(tmp_buf, 0, sizeof(tmp_buf));

    ginger_log(INFO, "%s\n", stats_buf);
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
