#include "emu_debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../shared/logger.h"
#include "../shared/print_utils.h"
#include "../shared/vector.h"

#define MAX_DEBUG_COMMAND_LEN 64

static void
emu_debug_examine_memory(risc_v_emu_t* emu, char input_buf[], char last_command[])
{
    memcpy(last_command, input_buf, MAX_DEBUG_COMMAND_LEN);

    // Get addresses to show from user input.
    printf("Address: ");
    char adr_buf[MAX_DEBUG_COMMAND_LEN];
    memset(adr_buf, 0, MAX_DEBUG_COMMAND_LEN);
    if (!fgets(adr_buf, MAX_DEBUG_COMMAND_LEN, stdin)) {
        ginger_log(ERROR, "Could not get user input!\n");
        abort();
    }
    const size_t mem_address = strtoul(adr_buf, NULL, 16);

    // Get size letter from user input.
    printf("Format (b, h, w, g): ");
    const char size_letter = fgetc(stdin);
    fgetc(stdin); // Avoid having the '\n' interfering with the next read.
    if (size_letter == '\0' || size_letter == (char)-1) {
        ginger_log(ERROR, "Could not get user input!\n");
        abort();
    }

    // Get addresses to show from user input.
    printf("Range: ");
    char range_buf[MAX_DEBUG_COMMAND_LEN];
    memset(range_buf, 0, MAX_DEBUG_COMMAND_LEN);
    if (!fgets(range_buf, MAX_DEBUG_COMMAND_LEN, stdin)) {
        ginger_log(ERROR, "Could not get user input!\n");
        abort();
    }
    const size_t mem_range = strtoul(range_buf, NULL, 10);

    print_emu_memory(emu, mem_address, mem_range, size_letter);
}

// Search emulator memory for user specified value.
static void
emu_debug_search_in_memory(risc_v_emu_t* emu)
{
    printf("Search for value: ");

    // Get value to search for from user input.
    char search_buf[MAX_DEBUG_COMMAND_LEN];
    memset(search_buf, 0, MAX_DEBUG_COMMAND_LEN);
    if (!fgets(search_buf, MAX_DEBUG_COMMAND_LEN, stdin)) {
        ginger_log(ERROR, "Could not get user input!\n");
        abort();
    }
    const size_t needle = strtoul(search_buf, NULL, 16);

    // Get size letter from user input.
    printf("Format (b, h, w, g): ");
    const char size_letter = fgetc(stdin);
    fgetc(stdin); // Avoid having the '\n' interfering with the next read.
    if (size_letter == '\0' || size_letter == (char)-1) {
        ginger_log(ERROR, "Could not get user input!\n");
        abort();
    }

    vector_t* search_result = emu->mmu->search(emu->mmu, needle, size_letter);
    if (search_result) {
        ginger_log(DEBUG, "%zu hit(s) of 0x%zx\n", vector_length(search_result), needle);
        for (size_t i = 0; i < search_result->length; i++) {
            ginger_log(DEBUG, "%zu: 0x%lx\n", i + 1, *(size_t*)vector_get(search_result, i));
        }
        vector_destroy(search_result);
    }
    else {
        ginger_log(DEBUG, "Did not find 0x%zx in emulator memory\n", needle);
    }
}

void
debug_emu(risc_v_emu_t* emu)
{
    static char last_command[MAX_DEBUG_COMMAND_LEN];

    for (;;) {
        printf("(debug) ");

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
        // We got a new command. Use it.
        else {
            memcpy(last_command, input_buf, MAX_DEBUG_COMMAND_LEN);
        }

        // New command is memory command.
        if (strcmp(input_buf, "x\n") == 0) {
            emu_debug_examine_memory(emu, input_buf, last_command);
        }

        // Show emulator register state.
        if (strcmp(input_buf, "r\n") == 0) {
            print_emu_registers(emu);
        }

        // Search for a value in emulator memory.
        if (strcmp(input_buf, "s\n") == 0) {
            emu_debug_search_in_memory(emu);
        }

        // Execute next instruction.
        if (strcmp(input_buf, "ni\n") == 0) {
            break;
        }

        if ((strcmp(input_buf, "exit\n") == 0) ||
            (strcmp(input_buf, "Exit\n") == 0) ||
            (strcmp(input_buf, "quit\n") == 0) ||
            (strcmp(input_buf, "Quit\n") == 0) ||
            (strcmp(input_buf, "q\n") == 0)) {
            exit(0);
        }
    }
}
