#include "debug_cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../shared/cli.h"
#include "../shared/logger.h"
#include "../shared/print_utils.h"
#include "../shared/vector.h"

#define MAX_NB_BREAKPOINTS           256

static const char* debug_instructions = ""                  \
    "Available CLI commands:\n"                             \
    " x - Examine emulator memory.\n"                       \
    " s - Search for value in emulator memory.\n"           \
    " n - Execute next instruction.\n"                      \
    " r - Show emulator registers.\n"                       \
    " b - Set breakpoint.\n"                                \
    " d - Show all breakpoints.\n"                          \
    " c - Run emulator until breakpoint or program exit.\n" \
    " h - Print this help.\n"                               \
    " q - Quit debugging and exit this program.\n";

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

    // Get breakpoint address from user.
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

static void
emu_debug_print_help(void)
{
    printf("%s", debug_instructions);
}

static void
emu_debug_quit(void)
{
    exit(0);
}

cli_t*
emu_debug_create_cli(risc_v_emu_t* emu)
{
    struct cli_cmd debug_cli_commands[] = {
        {
            .cmd_str = "x",
            .description = "Examine emulator memory.\n",
            .cmd_fn = emu_debug_examine_memory
        },
        {
            .cmd_str = "search memory",
            .description = "Search for value in emulator memory.\n",
            .cmd_fn = emu_debug_search_in_memory
        },
        {
            .cmd_str = "next instruction",
            .description = "Execute next instruction.\n",
            .cmd_fn = emu->execute
        },
        {
            .cmd_str = "show registers",
            .description = "Show emulator registers.\n",
            .cmd_fn = print_emu_registers
        },
        {
            .cmd_str = "break",
            .description = "Set breakpoint.\n",
            .cmd_fn = emu_debug_set_breakpoint
        },
        {
            .cmd_str = "show breakpoints",
            .description = "Show all breakpoints.\n",
            .cmd_fn = emu_debug_show_breakpoints
        },
        {
            .cmd_str = "continue",
            .description = "Run emulator until breakpoint or program exit.\n",
            .cmd_fn = emu_debug_run_until_breakpoint
        },
        {
            .cmd_str = "help",
            .description = "Print this help.\n",
            .cmd_fn = emu_debug_print_help
        },
        {
            .cmd_str = "quit",
            .description = "Quit debugging and exit this program.\n",
            .cmd_fn = emu_debug_quit
        }
    };

    const char* prompt_str = "(debug) ";
    cli_t* debug_cli = cli_create(prompt_str);

    for (int i = 0; i < sizeof(debug_cli_commands) / sizeof(debug_cli_commands[0]); i++) {
        cli_add_command(debug_cli, debug_cli_commands[i]);
    }

    return debug_cli;
}

void
debug_emu(risc_v_emu_t* emu, cli_t* cli)
{
    //static char last_command[MAX_LENGTH_DEBUG_CLI_COMMAND];
    //vector_t* breakpoints = vector_create(sizeof(uint64_t));

    for (;;) {
        cli->print_prompt(cli);
        cli->get_user_input(cli);

        /*
        // Get input from the user.
        char input_buf[MAX_LENGTH_DEBUG_CLI_COMMAND];
        memset(input_buf, 0, MAX_LENGTH_DEBUG_CLI_COMMAND);
        if (!fgets(input_buf, MAX_LENGTH_DEBUG_CLI_COMMAND, stdin)) {
            ginger_log(ERROR, "Could not get user input!\n");
            abort();
        }

        debug_emu_complete_command(input_buf);


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
        // Search for a value in emulator memory.
        if (strstr(input_buf, "s")) {
            emu_debug_search_in_memory(emu);
        }
        // Execute next instruction.
        if (strstr(input_buf, "n")) {
            emu->execute(emu);
        }
        // Show emulator register state.
        if (strstr(input_buf, "r")) {
            print_emu_registers(emu);
        }
        // Set breakpoint.
        if (strstr(input_buf, "b")) {
            emu_debug_set_breakpoint(emu, breakpoints);
        }
        // Show breakpoints.
        if (strstr(input_buf, "d")) {
            emu_debug_show_breakpoints(emu, breakpoints);
        }
        // Run until we hit a breakpoint or exit.
        if (strstr(input_buf, "c")) {
            emu_debug_run_until_breakpoint(emu, breakpoints);
        }
        // Show debug help.
        if (strstr(input_buf, "h")) {
            printf("%s", debug_instructions);
        }
        // Quit debugging.
        if (strcmp(input_buf, "q\n") == 0) {
            exit(0);
        }
        */
    }
}
