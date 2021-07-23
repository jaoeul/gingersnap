#include "debug_cli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../shared/logger.h"
#include "../shared/print_utils.h"
#include "../shared/vector.h"

#define MAX_LENGTH_DEBUG_CLI_COMMAND 64
#define MAX_NB_BREAKPOINTS           256
#define DEBUG_PROMPT_STR             "(debug) "

static const char cli_commands[][MAX_LENGTH_DEBUG_CLI_COMMAND] = {
    "x",
    "search memory",
    "next instruction",
    "show registers",
    "break",
    "show breakpoints",
    "continue",
    "help",
    "quit"
};

enum ARROW_KEYS {
    ARROW_UP    = 1,
    ARROW_DOWN  = 2,
    ARROW_RIGHT = 3,
    ARROW_LEFT  = 4
};

// Used to store the original terminal attributes when we enter raw mode.
struct termios orig_termios;

/*
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

*/
static void
disable_raw_mode()
{
    // Restore the original terminal attributes.
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

// Raw mode turns of character echoing and line buffering in the terminal.
static void
enable_raw_mode()
{
    // Store the original terminal attributes.
    tcgetattr(STDIN_FILENO, &orig_termios);

    // In case we exit in raw mode, restore the original terminal attributes.
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;

    raw.c_lflag &= ~ICANON; // Turn off line buffering (canonical mode).
    raw.c_lflag &= ~ECHO;   // Don't echo characters to the terminal output.
    //raw.c_lflag &= ~ECHOE;

    // Apply new terminal attributes.
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Entering an arrow key sends 3 chars. Esc, [, and
// either one of the values in the below switch case.
static int
is_arrow_key(const char key)
{
    if (key == 27) {
        char char2 = getc(stdin);
        if (char2 == 91) {
            char char3 = getc(stdin);
            switch (char3) {
                case 65:
                    return ARROW_UP;
                    break;
                case 66:
                    return ARROW_DOWN;
                    break;
                case 67:
                    return ARROW_RIGHT;
                    break;
                case 68:
                    return ARROW_LEFT;
                    break;
            }
        }
    }
    return 0;
}

/*
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

// TODO: Use this. Currently unused.
__attribute__((used))
static bool
parse_debug_cli_command(char* cli_cmd)
{
    // TODO: Memory corruption in this strip. Use strlen - 1 = '\0' instead.
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
*/

static bool
str_starts_with(const char* string, const char* substr)
{
    const size_t string_len = strlen(string);
    const size_t substr_len = strlen(substr);

    if (string_len > substr_len) {
        if (memcmp(substr, string, substr_len) == 0) {
            return true;
        }
    }
    return false;
}

static size_t
str_matching_chars(const char* str1, const char* str2)
{
    const size_t str1_len    = strlen(str1);
    const size_t str2_len    = strlen(str2);
    size_t shortest_len      = 0;
    size_t nb_matching_chars = 0;

    if (str1_len < str2_len) {
        shortest_len = str1_len;
    }
    else {
        shortest_len = str2_len;
    }
    for (size_t i = 0; i < shortest_len; i++) {
        if (str1[i] == str2[i]) {
            nb_matching_chars++;
        }
    }
    return nb_matching_chars;
}

// Used for tab completion and to execute the correct command if only a substring is given.
static void
debug_emu_complete_command(const char* substr, char** completion)
{
    uint8_t nb_cli_commands = sizeof(cli_commands) / MAX_LENGTH_DEBUG_CLI_COMMAND;
    uint8_t nb_hits         = 0;
    size_t  substr_len      = strlen(substr);
    char    to_complete[substr_len + 1];
    uint8_t hits[nb_cli_commands];
    memset(hits, 0, nb_cli_commands);

    // Copy substr and its nullbyte to to_complete.
    memcpy(to_complete, substr, substr_len + 1);

    // Strip newline from to_complete.
    size_t to_complete_len = strlen(to_complete) + 1;
    to_complete[to_complete_len] = '\0';

    // Note which commands were hit, and how many.
    for (uint8_t i = 0; i < nb_cli_commands; i++) {
        if (str_starts_with(cli_commands[i], to_complete)) {
            hits[nb_hits++] = i;
        }
    }

    if (nb_hits == 1) {
        // Single hit. Go ahead and complete the command.
        const char* hit_command = cli_commands[hits[0]];
        size_t matched_str_len = strspn(hit_command, to_complete);
        sprintf(*completion, "%s", hit_command + matched_str_len);
        return;
    }
    else if (nb_hits > 1) {
        // Multiple hits. Complete command as far as possible.
        size_t  nb_common_chars = MAX_LENGTH_DEBUG_CLI_COMMAND;
        uint8_t match_idx       = 0;

        // TODO: See if we can lower time complexity here.
        for (uint8_t i = 0; i < nb_hits; i++) {
            for (uint8_t j = 0; j < nb_hits; j++) {
                if (i == j) {
                    continue;
                }
                size_t nb_curr_match = str_matching_chars(cli_commands[hits[i]], cli_commands[hits[j]]);
                if (nb_curr_match < nb_common_chars) {
                    nb_common_chars = nb_curr_match;
                    match_idx = hits[i];
                }
            }
        }

        // Get the common chars.
        char matched_chars[nb_common_chars + 1];
        memcpy(matched_chars, cli_commands[match_idx], nb_common_chars);
        matched_chars[nb_common_chars] = '\0';

        // How much of the matched string have already been inputed, and can be ignored?
        size_t ignore = strlen(to_complete);
        sprintf(*completion, "%s", matched_chars + ignore);
    }
}

static void
debug_emu_show_completions(const char* substr)
{
    uint8_t nb_cli_commands = sizeof(cli_commands) / MAX_LENGTH_DEBUG_CLI_COMMAND;
    size_t  substr_len = strlen(substr);
    char    to_complete[substr_len + 1];

    // Copy substr and its nullbyte to to_complete.
    memcpy(to_complete, substr, substr_len + 1);

    // Strip newline from to_complete.
    size_t to_complete_len = strlen(to_complete) + 1;
    to_complete[to_complete_len] = '\0';

    // Show which commands were hit, and how many.
    printf("\n");
    for (uint8_t i = 0; i < nb_cli_commands; i++) {
        if (str_starts_with(cli_commands[i], to_complete)) {
            printf("%s\n", cli_commands[i]);
        }
    }
    printf("\n");
    fflush(stdout);
}

static void
debug_emu_print_prompt()
{
    printf(DEBUG_PROMPT_STR);
    fflush(stdout);
}

// TODO: Use this function to edit the cli debug command.
static bool
str_insert_char(const char* input, const char ins_char, const size_t idx, char** output)
{
    const size_t input_len = strlen(input);
    if (idx > input_len) {
        return false;
    }
    memcpy(*output, input, idx);
    (*output)[idx] = ins_char;
    memcpy(*output + idx + 1, input + idx, input_len - idx);
    memset(*output + (input_len + 1), '\0', 1);
    return true;
}

static void
debug_emu_get_user_input()
{
    char input_buf[MAX_LENGTH_DEBUG_CLI_COMMAND] = {0};
    size_t nb_read   = 0;
    char   prev_char = '\0';

    enable_raw_mode();
    for (;;) {
        char curr_char = '\0';
        size_t read_result = read(STDIN_FILENO, &curr_char, 1);
        if (read_result != 1 || curr_char == '\0') {
            ginger_log(ERROR, "Could not read char from stdin!\n");
            return;
        }

        if (is_arrow_key(curr_char) != 0) {
            // Ignore the arrow keys. Might want to change this to make the user
            // able to manouver the cursor left and right when editing the debug
            // command.
            continue;
        }
        else if (prev_char == '\t' && curr_char == '\t') {
            // Double tab. Show completion options.
            debug_emu_show_completions(input_buf);
            debug_emu_print_prompt();
            printf("%s", input_buf);
            fflush(stdout);
            continue;
        }
        else if (curr_char == '\t') {
            // Do the autocompletion here.
            char* completion = calloc(MAX_LENGTH_DEBUG_CLI_COMMAND, sizeof(char));
            debug_emu_complete_command(input_buf, &completion);
            printf("%s", completion);
            // Add the completion to the input_buf.
            const size_t comp_len = strlen(completion);
            strncat(input_buf, completion, comp_len);
            nb_read += comp_len;
            fflush(stdout);
            free(completion);
        }
        else if (curr_char == '\n') {
            printf("\n");
            fflush(stdout);
            return;
        }
        else if (curr_char == 127) {
            // Handle backspace.
            if (nb_read > 0) {
                char spaces[nb_read + 1];
                memset(spaces, 32, nb_read);
                spaces[nb_read] = '\0';
                printf("\r%s%s", DEBUG_PROMPT_STR, spaces);
                nb_read--;
                input_buf[nb_read] = '\0';
                printf("\r%s%s", DEBUG_PROMPT_STR, input_buf);
                fflush(stdout);
            }
            continue;
        }
        // TODO: Fix stack buffer overflow.
        else {
            printf("%c", curr_char);
            fflush(stdout);
            input_buf[nb_read++] = curr_char;
        }
        prev_char = curr_char;
    }
    disable_raw_mode();
}

void
debug_emu(risc_v_emu_t* emu)
{
    //static char last_command[MAX_LENGTH_DEBUG_CLI_COMMAND];
    //vector_t* breakpoints = vector_create(sizeof(uint64_t));

    for (;;) {
        debug_emu_print_prompt();

        debug_emu_get_user_input();

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
