#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../emu/risc_v_emu.h"
#include "../shared/elf_loader.h"
#include "../shared/endianess_converter.h"
#include "../shared/endianess_converter.h"
#include "../shared/logger.h"
#include "../shared/print_utils.h"

#define MAX_DEBUG_COMMAND_LEN 64

// Give the user the ability to show values in the emulator memory, print
// emulator register state or go to the next instruction. If no debug command
// was entered, we execute the last entered command, like GDB.
__attribute__((used))
    static void
debug_emu(risc_v_emu_t* emu)
{
    static char last_command[MAX_DEBUG_COMMAND_LEN];

    for (;;) {
        // Get input from the user.
        char input_buf[MAX_DEBUG_COMMAND_LEN];
        memset(input_buf, 0, MAX_DEBUG_COMMAND_LEN);
        if (!fgets(input_buf, MAX_DEBUG_COMMAND_LEN, stdin)) {
            ginger_log(ERROR, "Could not get user input!\n");
            abort();
        }

        // We got no new command, resue the last one.
        if (input_buf[0] == '\n') {
            memcpy(input_buf, last_command, MAX_DEBUG_COMMAND_LEN);
        }
        else {
            memcpy(last_command, input_buf, MAX_DEBUG_COMMAND_LEN);
        }

        // New command is memory command, and last one was not.
        if (strstr(input_buf, "m")) {
            memcpy(last_command, input_buf, MAX_DEBUG_COMMAND_LEN);

            // Get addresses to show from user input.
            printf("Address: ");
            char adr_buf[MAX_DEBUG_COMMAND_LEN];
            memset(adr_buf, 0, MAX_DEBUG_COMMAND_LEN);
            if (!fgets(adr_buf, MAX_DEBUG_COMMAND_LEN, stdin)) {
                ginger_log(ERROR, "Could not get user input!\n");
                abort();
            }
            const size_t mem_address = strtoul(adr_buf, NULL, 10);

            // Get addresses to show from user input.
            printf("Range: ");
            char range_buf[MAX_DEBUG_COMMAND_LEN];
            memset(range_buf, 0, MAX_DEBUG_COMMAND_LEN);
            if (!fgets(range_buf, MAX_DEBUG_COMMAND_LEN, stdin)) {
                ginger_log(ERROR, "Could not get user input!\n");
                abort();
            }
            const size_t mem_range = strtoul(range_buf, NULL, 10);

            print_emu_memory(emu, mem_address, mem_range);
        }

        // Show emulator register state.
        if (strstr(input_buf, "r")) {
            print_emu_registers(emu);
        }

        // Execute next instruction.
        if (strstr(input_buf, "ni")) {
            break;
        }
    }
}

static void
run_emu(risc_v_emu_t* emu)
{
    for (;;) {

#ifdef EMU_MODE_DEBUG
        debug_emu(emu);
#endif
        emu->execute(emu);
    }
}

int
main(int argc, char** argv)
{
    const size_t emu_total_mem = (1024 * 1024) * 256;
    const size_t stack_size    = (1024 * 1024);

    risc_v_emu_t* emu = risc_v_emu_create(emu_total_mem);

    // Load the elf into emulator memory.
    // Virtual addresses in the elf program headers corresponds directly to
    // addresses in the emulators memory.
    load_elf("./target", emu);

    // Create a stack which starts at the current_allocation address of the emulator
    uint64_t stack = emu->mmu->allocate(emu->mmu, stack_size);
    emu->registers[REG_SP] = stack + stack_size;

    // Populate program name memory segment
    uint64_t program_name_address = emu->mmu->allocate(emu->mmu, 4096);
    ginger_log(INFO, "program_name_address: 0x%lx\n", program_name_address);
    emu->mmu->write(emu->mmu, program_name_address, (uint8_t*)"ls\0", 3);

    // Populate arg1 name memory segment
    uint64_t argv1_address = emu->mmu->allocate(emu->mmu, 4096);
    ginger_log(INFO, "argv1_address: 0x%lx\n", argv1_address);
    emu->mmu->write(emu->mmu, argv1_address, (uint8_t*)"arg1\0", 4);

    // Push initial values onto the stack
    ginger_log(INFO, "Building initial stack at guest address: 0x%x\n", emu->registers[REG_SP]);
    uint8_t auxp[8]     = {0};
    uint8_t envp[8]     = {0};
    uint8_t argv_end[8] = {0};
    uint8_t argv1[8]    = {0};
    uint8_t argv0[8]    = {0};

    u64_to_byte_arr(argv1_address, argv1, LSB);
    u64_to_byte_arr(program_name_address, argv0, LSB);

    // Push the required values onto the stack as 64 bit values
    emu->stack_push(emu, auxp, 8);
    emu->stack_push(emu, envp, 8);
    emu->stack_push(emu, argv_end, 8);
    emu->stack_push(emu, argv1, 8);
    emu->stack_push(emu, argv0, 8);

    if (argc > 1) {
        print_emu_memory_allocated(emu);
        print_emu_registers(emu);
    }

    ginger_log(INFO, "Current allocation address: 0x%lx\n", emu->mmu->current_allocation);

    run_emu(emu);

    ginger_log(INFO, "Destroying emu structs!\n");
    emu->destroy(emu);

    return 0;
}
