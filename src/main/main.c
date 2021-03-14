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
    risc_v_emu_t* src_emu = risc_v_emu_create(1024 * 1024);
    risc_v_emu_t* dst_emu = risc_v_emu_create(1024 * 1024);
    if (!src_emu || !dst_emu) {
        return 1;
    }
    src_emu->mmu->allocate(src_emu->mmu, 1024 * 16);
    dst_emu->mmu->allocate(dst_emu->mmu, 1024 * 16);

    for (uint64_t i = 0; i < 10; i++) {
        src_emu->mmu->write(src_emu->mmu, 0, (uint8_t*)"hejsan\0", 7);
    }

    // Fork the src_emu to the dst_emu
    dst_emu->fork(dst_emu, src_emu);

    printf("\n%s\n", (unsigned char*)src_emu->mmu->memory);
    for (size_t i = 0; i < 7; i++) {
        printf("%u ", src_emu->mmu->permissions[i]);
    }
    printf("\n");

    printf("\n%s\n", (unsigned char*)dst_emu->mmu->memory);
    for (size_t i = 0; i < 7; i++) {
        printf("%u ", dst_emu->mmu->permissions[i]);
    }
    printf("\n");

    load_elf("/usr/bin/ls", src_emu);

    printf("Destroying emu structs!\n");
    src_emu->destroy(src_emu);
    dst_emu->destroy(dst_emu);

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
