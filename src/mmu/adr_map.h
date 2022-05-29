#ifndef ADR_MAP_H
#define ADR_MAP_H

#include <stdint.h>

#include "../elf_loader/program_header.h"

typedef struct {
    uint64_t low;  // Inclusive.
    uint64_t high; // Inclusive.
} adr_map_t;

uint64_t
get_mapped(adr_map_t* maps, uint64_t nb_maps, uint64_t requested);

adr_map_t*
adr_map_create(const program_header_t* prg_hdr);

void
adr_map_destroy(adr_map_t* map);

#endif
