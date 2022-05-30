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

#include "adr_map.h"

#include "../elf_loader/program_header.h"

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
