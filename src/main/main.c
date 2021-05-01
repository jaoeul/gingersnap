#include <stdio.h>
#include <malloc.h>

#include "../emu/risc_v_emu.h"
#include "../shared/print_utils.h"
#include "../shared/elf_loader.h"
#include "../shared/endianess_converter.h"

int
main(void)
{
    const size_t emu_total_mem = (1024 * 1024) * 250;
    const size_t stack_size    = (1024 * 1024);

    printf("Engaging Gingersnap!\n");

    risc_v_emu_t* emu = risc_v_emu_create(emu_total_mem);

    // Load the elf into emulator memory.
    // Virtual addresses in the elf program headers corresponds directly to
    // addresses in the emulators memory.
    load_elf("/usr/bin/ls", emu);

    // Create a stack which starts at the current_allocation address of the emulator
    uint64_t stack = emu->mmu->allocate(emu->mmu, stack_size);
    emu->registers.sp = stack + stack_size;

    // Populate program name memory segment
    uint64_t program_name_address = emu->mmu->allocate(emu->mmu, 4096);
    printf("program_name_address: 0x%lx\n", program_name_address);
    emu->mmu->write(emu->mmu, program_name_address, (uint8_t*)"ls\0", 3);

    // Populate arg1 name memory segment
    uint64_t argv1_address = emu->mmu->allocate(emu->mmu, 4096);
    printf("argv1_address: 0x%lx\n", argv1_address);
    emu->mmu->write(emu->mmu, argv1_address, (uint8_t*)"arg1\0", 4);

    // Push initial values onto the stack
    printf("Building initial stack at guest address: 0x%lx\n", emu->registers.sp);
    uint8_t auxp[8]     = {0};
    uint8_t envp[8]     = {0};
    uint8_t argv_end[8] = {0};
    uint8_t argv1[8]    = {0};
    uint8_t argv0[8]    = {0};

    // Push the required values onto the stack as 64 bit values
    emu->stack_push(emu, auxp, 8);
    emu->stack_push(emu, envp, 8);
    emu->stack_push(emu, argv_end, 8);

    u64_to_byte_arr(argv1_address, argv1, LSB);
    emu->stack_push(emu, argv1, 8);

    u64_to_byte_arr(program_name_address, argv0, LSB);
    emu->stack_push(emu, argv0, 8);

    print_emu_memory_allocated(emu);
    print_emu_registers(emu);

    printf("Current allocation: 0x%lx\n", emu->mmu->current_allocation);
    printf("Destroying emu structs!\n");
    emu->destroy(emu);

    return 0;
}
