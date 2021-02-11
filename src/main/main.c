#include <stdio.h>
#include <malloc.h>

#include "../emu/risc_v_emu.h"

int
main(void)
{
    printf("Engaging Gingersnap!\n");

    // Create a emulator with 1MB of ram
    risc_v_emu_t* emu = risc_v_emu_create(1024 * 1024 * 1024);
    if (!emu) {
        return 1;
    }

    // TODO: malloc_usable_size() is not portable across plotforms
    printf("emu struct size: %ld bytes\n"
           "emu memory size: %ld bytes\n"
           "emu permissions size: %ld bytes\n"
           "emu struct address: %p\n",
           sizeof(*emu),
           malloc_usable_size(emu->mmu->memory),
           malloc_usable_size(emu->mmu->permissions),
           emu);

    // Allocate 1KB of memory for a emulator
    emu->mmu->allocate(emu->mmu, 1024);

    printf("Destroying emu struct!\n");
    emu->destroy(emu);

    return 0;
}
