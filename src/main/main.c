#include <stdio.h>
#include <malloc.h>

#include "../emu/risc_v_emu.h"
#include "../shared/print_utils.h"
#include "../shared/elf_loader.h"
#include "../shared/endianess_converter.h"

int
main(void)
{
    printf("Engaging Gingersnap!\n");

    // Create a emulator with 1MB of ram
    risc_v_emu_t* emu = risc_v_emu_create(1024 * 250);

    emu->mmu->allocate(emu->mmu, (1024 * 240));

    load_elf("/usr/bin/ls", emu);

    print_emu_memory_allocated(emu);
    print_emu_registers(emu);

    printf("Destroying emu structs!\n");
    emu->destroy(emu);

    return 0;
}

//    // TODO: malloc_usable_size() is not portable across platforms
//    printf("emu struct size: %ld bytes\n"
//           "emu memory size: %ld bytes\n"
//           "emu permissions size: %ld bytes\n"
//           "emu struct address: %p\n",
//           sizeof(*emu),
//           malloc_usable_size(emu->mmu->memory),
//           malloc_usable_size(emu->mmu->permissions),
//           (void*)emu);
//
//    // Allocate 1KB of memory for a emulator
//    emu->mmu->allocate(emu->mmu, 1024 * 10);
//
//    // Write some text to emulator memory
//    uint8_t* test_str= (unsigned char*)"test\0";
//    emu->mmu->write(emu->mmu, 0, test_str, sizeof(test_str));
//
//    uint8_t* test_str2 = (uint8_t*)"test2\0";
//    emu->mmu->write(emu->mmu, 1024, test_str2, sizeof(test_str2));
//
//    // Print the written text
//    printf("\n%s\n", (unsigned char*)emu->mmu->memory);
//
//    printf("Dirty_blocks: ");
//    emu->mmu->dirty_state->print(emu->mmu->dirty_state);
//
//    printf("Bitmaps:\n");
//    print_bitmaps(emu->mmu->dirty_state->dirty_bitmaps,
//                  emu->mmu->dirty_state->nb_max_dirty_bitmaps);
