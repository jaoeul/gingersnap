/**
 * This address translator removes virtual address offset of a loadable program
 * header.
 *
 * program_header[0..file_size] = program_header[virt_adr..virt_adr + file_size]
 *
 * This is needed when addresses becomes high, and we don't want to allocate
 * memory for all virtual addresses in our emulators.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../elf_loader/program_header.h"

#include "adr_map.h"

uint64_t
get_mapped(adr_map_t* maps, uint64_t nb_maps, uint64_t requested)
{
    for (uint64_t i = 0; i < nb_maps; i++) {
        if (requested >= maps[i].low && requested <= maps[i].high) {
            return requested - maps[i].low;
        }
    }
    printf("Did not find requested value in any address mapping\n");
    abort();
}

adr_map_t*
adr_map_create(const program_header_t* prg_hdr)
{
    adr_map_t* map = calloc(1, sizeof(adr_map_t));
    map->low       = prg_hdr->virtual_address;
    map->high      = prg_hdr->virtual_address + prg_hdr->file_size - 1;
    return map;
}

void
adr_map_destroy(adr_map_t* map)
{
    if (map) {
        free(map);
    }
}
