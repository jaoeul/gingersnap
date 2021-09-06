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

#define MAX_NB_BREAKPOINTS 256
#define MAX_LEN_REG_STR    4

// Required amount of characters to display the larges integer supported by an
// 64 bit system.
#define MAX_NB_EXAMINE_ADDRESSES 19

static const char reg_strs[][MAX_LEN_REG_STR] = { "ra", "sp", "gp", "tp", "t0", "t1", "t2", "fp", "s1", "a0", "a1",
                                                  "a2", "a3", "a4", "a5", "a6", "a7", "s2", "s3", "s4", "s5", "s6",
                                                  "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6", "pc" };

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

/*
// Test if value is a valid ascii number.
static bool
is_number(const char num)
{
    if (num > 47 && num < 58) {
        return true;
    }
    else {
        return false;
    }
}

// Test if value is a valid specifiers.
static bool
is_specifier(const char* specifiers, const char test, int nb_specifiers)
{
    for (int i = 0; i < nb_specifiers; i ++) {
        if (specifiers[i] == test){
            return true;
        }
    }
    return false;
}
*/

// TODO: Make this work.
//       Display emulotor memory contents.
static void
emu_debug_examine_memory(rv_emu_t* emu, const char* input_buf)
{
    /*
    // Parse the command.
    //
    // High level algorithm:
    //
    // If x is followed by a '/'
    //     if '/' is followed by a number
    //         get the formating specifier
    //
    // print provided memory address with the provided specifiers and the
    // correct amount (formatting specifier defaults to hex and amount
    // defaults to 1)
    ginger_log(INFO, "[%s] Got command %s\n", __func__, input_buf);

    char valid_specifiers[] = {'o', 'x', 'd', 'u', 't', 'f', 'a', 'i', 'c', 's', 'z'};
    int  nb_specifiers      = sizeof valid_specifiers / sizeof valid_specifiers[0];

    if (input_buf[1] == '/') {
        // Read decimal numbers until we get a valid formating specifier.
        char number_buf[MAX_NB_EXAMINE_ADDRESSES] = {0};
        for (size_t i = 2; i < MAX_NB_EXAMINE_ADDRESSES; i++) {

            // If character is a not valid number or if it is not a valid specifier, break.
            if (!is_number(input_buf[i]) || is_specifier(valid_specifiers, input_buf[i], nb_specifiers)) {
                break;
            }
            // Else, append the decimal ascii char to the number_buf.
            else {
                number_buf[i - 2] = input_buf[i];
            }
        }
        // Convert the amount of values to print from a char array to a uint64_t.
        uint64_t nb_to_print = strtoul(number_buf, NULL, 10);
        ginger_log(INFO, "nb_to_print: %lu\n", nb_to_print);
    }

    //const size_t mem_range = strtoul(range_buf, NULL, 10);
    //print_emu_memory(emu, mem_address, mem_range, size_letter);
    */

    //memcpy(last_command, input_buf, MAX_LENGTH_DEBUG_CLI_COMMAND);

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
emu_debug_search_in_memory(rv_emu_t* emu)
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

// TODO: Break on register watchpoints.
static void
emu_debug_run_until_breakpoint(rv_emu_t* emu, vector_t* breakpoints)
{
    for (;;) {
        emu->execute(emu);
        for (size_t i = 0; i < vector_length(breakpoints); i++) {
            uint64_t curr_pc = get_reg(emu, REG_PC);

            if (curr_pc == *(uint64_t*)vector_get(breakpoints, i)) {
                ginger_log(INFO, "Hit breakpoint %zu\t0x%zx\n", i, curr_pc);
                return;
            }
        }
    }
}

static void
emu_debug_show_breakpoints(rv_emu_t* emu, vector_t* breakpoints)
{
    size_t nb_breakpoints = vector_length(breakpoints);
    if (nb_breakpoints == 0) {
        printf("No breakpoints\n");
        return;
    }

    printf("\nBreakpoints:\n");
    for (size_t i = 0; i < nb_breakpoints; i++) {
        printf("%zu\t0x%zx\n", i, *(size_t*)vector_get(breakpoints, i));
    }
}

static void
emu_debug_show_watchpoints(rv_emu_t* emu, vector_t* watchpoints)
{
    size_t nb_watchpoints = vector_length(watchpoints);
    if (nb_watchpoints == 0) {
        printf("No watchpoints\n");
        return;
    }

    printf("\nWatchpoints:\n");
    for (size_t i = 0; i < nb_watchpoints; i++) {
        char* curr_watchpoint = vector_get(watchpoints, i);
        printf("%zu\t%s\n", i, curr_watchpoint);
    }
}

static void
emu_debug_set_breakpoint(rv_emu_t* emu, vector_t* breakpoints)
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

static bool
emu_debug_set_watchpoint(rv_emu_t* emu, vector_t* watchpoints)
{
    printf("\nSet watchpoint on register: ");

    // Get register to watch from user.
    char watchpoint_buf[MAX_LEN_REG_STR];
    memset(watchpoint_buf, 0, MAX_LEN_REG_STR);

    if (!fgets(watchpoint_buf, MAX_LEN_REG_STR, stdin)) {
        ginger_log(ERROR, "Could not get user input!\n");
        abort();
    }
    // TODO: Dedup watchpoint vector.

    // Strip newline.
    size_t watch_len = strlen(watchpoint_buf);
    watchpoint_buf[watch_len - 1] = '\0';

    for (uint8_t i = 0; i < sizeof reg_strs / sizeof reg_strs[0]; i++) {
        if (strncmp(watchpoint_buf, reg_strs[i], MAX_LEN_REG_STR) == 0) {
            vector_append(watchpoints, (char*)watchpoint_buf);
            printf("true\n");
            return true;
        }
    }
    printf("false\n");
    return false;
}

cli_t*
emu_debug_create_cli(rv_emu_t* emu)
{
    struct cli_cmd debug_cli_commands[] = {
        {
            .cmd_str = "x",
            .description = "Examine emulator memory.\n"
        },
        {
            .cmd_str = "search memory",
            .description = "Search for value in emulator memory.\n"
        },
        {
            .cmd_str = "next instruction",
            .description = "Execute next instruction.\n"
        },
        {
            .cmd_str = "info registers",
            .description = "Show emulator registers.\n"
        },
        {
            .cmd_str = "break",
            .description = "Set breakpoint.\n"
        },
        {
            .cmd_str = "watch",
            .description = "Set register watchpoint.\n"
        },
        {
            .cmd_str = "show breakpoints",
            .description = "Show all breakpoints.\n"
        },
        {
            .cmd_str = "show watchpoints",
            .description = "Show all watchpoints.\n"
        },
        {
            .cmd_str = "continue",
            .description = "Run emulator until breakpoint or program exit.\n"
        },
        {
            .cmd_str = "help",
            .description = "Print this help.\n"
        },
        {
            .cmd_str = "quit",
            .description = "Quit debugging and exit this program.\n"
        }
    };

    const char* prompt_str = "(debug) ";
    cli_t* debug_cli = cli_create(prompt_str);

    for (int i = 0; i < sizeof(debug_cli_commands) / sizeof(debug_cli_commands[0]); i++) {
        debug_cli->add_command(debug_cli, debug_cli_commands[i]);
    }

    return debug_cli;
}

void
debug_emu(rv_emu_t* emu, cli_t* cli)
{
    static char last_command[MAX_LENGTH_DEBUG_CLI_COMMAND];
    vector_t*   breakpoints = vector_create(sizeof(uint64_t));
    vector_t*   watchpoints = vector_create(sizeof(MAX_LEN_REG_STR));

    for (;;) {
        printf("\n");
        cli->print_prompt(cli);

        // Can only return valid command string.
        char* command_str = cli->get_command(cli);

        // We got no new command.
        if (!command_str) {
            // If there is no previously entered command, skip this iteration.
            if (strlen(last_command) > 1) {
                command_str = calloc(MAX_LENGTH_DEBUG_CLI_COMMAND, sizeof(char));
                memcpy(command_str, last_command, MAX_LENGTH_DEBUG_CLI_COMMAND);
            }
            // If we got no new command, and we have no previous command.
            else {
                continue;
            }
        }
        // New command is memory command.
        if (strncmp(command_str, "x", 1) == 0) {
            emu_debug_examine_memory(emu, command_str);
        }
        // Search for a value in emulator memory.
        else if (strncmp(command_str, "search memory", 13) == 0) {
            emu_debug_search_in_memory(emu);
        }
        // Execute next instruction.
        else if (strncmp(command_str, "next instruction", 16) == 0) {
            emu->execute(emu);
        }
        // Show emulator register state.
        else if (strncmp(command_str, "info registers", 14) == 0) {
            print_emu_registers(emu);
        }
        // Set breakpoint.
        else if (strncmp(command_str, "break", 5) == 0) {
            emu_debug_set_breakpoint(emu, breakpoints);
        }
        // Set register watchpoint.
        else if (strncmp(command_str, "watch", 5) == 0) {
            emu_debug_set_watchpoint(emu, watchpoints);
        }
        // Show breakpoints.
        else if (strncmp(command_str, "show breakpoints", 16) == 0) {
            emu_debug_show_breakpoints(emu, breakpoints);
        }
        // Show watchpoints.
        else if (strncmp(command_str, "show watchpoints", 16) == 0) {
            emu_debug_show_watchpoints(emu, watchpoints);
        }
        // Run until we hit a breakpoint or exit.
        else if (strncmp(command_str, "continue", 8) == 0) {
            emu_debug_run_until_breakpoint(emu, breakpoints);
        }
        // Show debug help.
        else if (strncmp(command_str, "help", 4) == 0) {
            printf("%s", debug_instructions);
        }
        // Quit debugging.
        else if (strncmp(command_str, "quit", 4) == 0) {
            exit(0);
        }

        // Save executed command as previous command.
        memcpy(last_command, command_str, MAX_LENGTH_DEBUG_CLI_COMMAND);

        // Free the heap allocated user input string.
        cli->free_user_input(command_str);
    }
    vector_destroy(breakpoints);
    vector_destroy(watchpoints);
}
