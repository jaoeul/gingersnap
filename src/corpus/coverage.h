#ifndef COVERAGE_H
#define COVERAGE_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_NB_COVERAGE_HASHES 1024

typedef struct {
    uint64_t from;
    uint64_t to;
} coverage_hash_key_t;

typedef struct {
    // Tracks which branches have been taken. Using hashes makes sure that we
    // do not cover duplicate branches.
    uint8_t hashes[MAX_NB_COVERAGE_HASHES];
} coverage_t;

// Returns true and marks the branch as covered if it has not been taken before.
// Otherwise, return false.
bool
coverage_on_branch(coverage_t* cov, uint64_t from, uint64_t to);

coverage_t*
coverage_create(void);

void
coverage_destroy(coverage_t* coverage);

#endif
