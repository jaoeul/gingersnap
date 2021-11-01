#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "coverage.h"

#include "../main/config.h"
#include "../shared/hash.h"
#include "../shared/logger.h"

#define COVERAGE_NOT_COVERED 0
#define COVERAGE_COVERED     1

// Defined in `main.c`.
extern global_config_t global_config;

bool
coverage_on_branch(coverage_t* cov, uint64_t from, uint64_t to)
{
    const coverage_hash_key_t key = {
        .from = from,
        .to   = to,
    };
    const uint32_t hash = murmur3_32((uint8_t*)&key, sizeof(coverage_hash_key_t), 0) % MAX_NB_COVERAGE_HASHES;
    return __sync_bool_compare_and_swap(&cov->hashes[hash], COVERAGE_NOT_COVERED, COVERAGE_COVERED, __ATOMIC_SEQ_CST);
}

coverage_t*
coverage_create(void)
{
    coverage_t* coverage = calloc(1, sizeof(coverage_t));
    if (!coverage) {
        ginger_log(ERROR, "[%s] Could not allocate memory for coverage!\n", __func__);
        abort();
    }
    return coverage;
}

void
coverage_destroy(coverage_t* coverage)
{
    if (coverage) {
        free(coverage);
    }
}
