#include "debug_cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../shared/logger.h"
#include "../shared/print_utils.h"
#include "../shared/vector.h"

#define MAX_LENGTH_DEBUG_CLI_COMMAND 64
#define MAX_NB_BREAKPOINTS           256

static const char* debug_instructions = ""        \
    "Available CLI commands:\n"              \
    " h - Print this help.\n"                      \
    " x - Examine emulator memory.\n"             \
    " s - Search for value in emulator memory.\n" \
    " n - Execute next instruction.\n"            \
    " r - Show emulator registers.\n"             \
    " q - Quit debugging and exit this program.\n";

__attribute__((used))
static const char cli_commands[][MAX_LENGTH_DEBUG_CLI_COMMAND] = {
    "h",
    "x",
    "s",
    "n",
    "r",
    "q"
};

static void
emu_debug_examine_memory(risc_v_emu_t* emu, char input_buf[], char last_command[])
{
    memcpy(last_command, input_buf, MAX_LENGTH_DEBUG_CLI_COMMAND);

    // Get addresses to show from user input.
    printf("Address: ");
    char adr_buf[MAX_LENGTH_DEBUG_CLI_COMMAND];
    memset(adr_buf, 0, MAX_LENGTH_DEBUG_CLI_COMMAND);
    if (!fgets(adr_buf, MAX_LENGTH_DEBUG_CLI_COMMAND, stdin)) {
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
    char range_buf[MAX_LENGTH_DEBUG_CLI_COMMAND];
    memset(range_buf, 0, MAX_LENGTH_DEBUG_CLI_COMMAND);
    if (!fgets(range_buf, MAX_LENGTH_DEBUG_CLI_COMMAND, stdin)) {
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
    char search_buf[MAX_LENGTH_DEBUG_CLI_COMMAND];
    memset(search_buf, 0, MAX_LENGTH_DEBUG_CLI_COMMAND);
    if (!fgets(search_buf, MAX_LENGTH_DEBUG_CLI_COMMAND, stdin)) {
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

static void
emu_debug_run_until_breakpoint(risc_v_emu_t* emu, vector_t* breakpoints)
{
    for (;;) {
        emu->execute(emu);
        for (size_t i = 0; i < vector_length(breakpoints); i++) {
            uint64_t curr_pc = get_register(emu, REG_PC);

            if (curr_pc == *(uint64_t*)vector_get(breakpoints, i)) {
                ginger_log(INFO, "Hit breakpoint %zu\t0x%zx\n", i, curr_pc);
                return;
            }
        }
    }
}

static void
emu_debug_show_breakpoints(risc_v_emu_t* emu, vector_t* breakpoints)
{
    size_t nb_breakpoints = vector_length(breakpoints);
    if (nb_breakpoints == 0) {
        printf("No breakpoints\n");
        return;
    }

    printf("Breakpoints:\n");
    for (size_t i = 0; i < nb_breakpoints; i++) {
        printf("%zu\t0x%zx\n", i, *(size_t*)vector_get(breakpoints, i));
    }
}

static void
emu_debug_set_breakpoint(risc_v_emu_t* emu, vector_t* breakpoints)
{
    printf("Set breakpoint at address: ");

    // Get value to search for from user input.
    char break_adr_buf[MAX_LENGTH_DEBUG_CLI_COMMAND];
    memset(break_adr_buf, 0, MAX_LENGTH_DEBUG_CLI_COMMAND);
    if (!fgets(break_adr_buf, MAX_LENGTH_DEBUG_CLI_COMMAND, stdin)) {
        ginger_log(ERROR, "Could not get user input!\n");
        abort();
    }
    size_t break_adr = strtoul(break_adr_buf, NULL, 16);

    if (break_adr > emu->mmu->memory_size) {
        ginger_log(ERROR, "Could not set breakpoint at 0x%zx as it is outside of emulator memory!\n", break_adr);
        return;
    }

    if ((emu->mmu->permissions[break_adr] & PERM_EXEC) == 0) {
        ginger_log(ERROR, "Could not set breakpoint at 0x%zx! No execute permissions!\n", break_adr);
        return;
    }

    vector_append(breakpoints, &break_adr);
}

// TODO: Use this. Currently unused.
__attribute__((used))
static bool
parse_debug_cli_command(char* cli_cmd)
{
    // Strip newline.
    size_t strip_size = strcspn(cli_cmd, "\n");
    char stripped[strip_size];
    memcpy(stripped, cli_cmd, strip_size);

    const uint8_t nb_valid_cli_commands = sizeof(cli_commands) / MAX_LENGTH_DEBUG_CLI_COMMAND;
    for (const char* i = cli_commands[0]; i < cli_commands[nb_valid_cli_commands]; i += MAX_LENGTH_DEBUG_CLI_COMMAND) {
        if (strcmp(stripped, i) == 0) {
            return true;
        }
    }
    return false;
}

void
debug_emu(risc_v_emu_t* emu)
{
    static char last_command[MAX_LENGTH_DEBUG_CLI_COMMAND];
    vector_t* breakpoints = vector_create(sizeof(uint64_t));

    for (;;) {
        printf("(debug) ");

        // Get input from the user.
        char input_buf[MAX_LENGTH_DEBUG_CLI_COMMAND];
        memset(input_buf, 0, MAX_LENGTH_DEBUG_CLI_COMMAND);
        if (!fgets(input_buf, MAX_LENGTH_DEBUG_CLI_COMMAND, stdin)) {
            ginger_log(ERROR, "Could not get user input!\n");
            abort();
        }

        // We got no new command, resue the last one.
        if (input_buf[0] == '\n') {
            memcpy(input_buf, last_command, MAX_LENGTH_DEBUG_CLI_COMMAND);
        }
        // We got a new command. Use it.
        else {
            memcpy(last_command, input_buf, MAX_LENGTH_DEBUG_CLI_COMMAND);
        }

        // New command is memory command.
        if (strstr(input_buf, "x")) {
            emu_debug_examine_memory(emu, input_buf, last_command);
        }

        // Show emulator register state.
        if (strstr(input_buf, "r")) {
            print_emu_registers(emu);
        }

        // Search for a value in emulator memory.
        if (strstr(input_buf, "s")) {
            emu_debug_search_in_memory(emu);
        }

        // Execute next instruction.
        if (strstr(input_buf, "n")) {
            emu->execute(emu);
        }

        // Run until we hit a breakpoint or exit.
        if (strstr(input_buf, "c")) {
            emu_debug_run_until_breakpoint(emu, breakpoints);
        }

        if (strstr(input_buf, "b")) {
            emu_debug_set_breakpoint(emu, breakpoints);
        }

        if (strstr(input_buf, "d")) {
            emu_debug_show_breakpoints(emu, breakpoints);
        }

        if (strstr(input_buf, "h")) {
            printf("%s", debug_instructions);
        }

        if (strcmp(input_buf, "q\n") == 0) {
            exit(0);
        }
    }
}
