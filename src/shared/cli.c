#include "cli.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../shared/logger.h"
#include "../shared/vector.h"

#define MAX_NB_BREAKPOINTS           256

enum ARROW_KEYS {
    ARROW_UP    = 1,
    ARROW_DOWN  = 2,
    ARROW_RIGHT = 3,
    ARROW_LEFT  = 4
};

// Used to store the original terminal attributes when we enter raw mode.
struct termios orig_termios;

static void
cli_disable_raw_mode()
{
    // Restore the original terminal attributes.
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

// Raw mode turns of character echoing and line buffering in the terminal.
static void
cli_enable_raw_mode()
{
    // Store the original terminal attributes.
    tcgetattr(STDIN_FILENO, &orig_termios);

    // In case we exit in raw mode, restore the original terminal attributes.
    atexit(cli_disable_raw_mode);

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
cli_is_arrow_key(const char key)
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
// TODO: Use this. Currently unused.
static bool
cli_parse_command(char* cli_cmd)
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
cli_str_starts_with(const char* string, const char* substr)
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
cli_str_matching_chars(const char* str1, const char* str2)
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
cli_complete_command(const cli_t* cli, const char* substr, char** completion)
{
    uint8_t nb_cli_commands = vector_length(cli->commands);
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
        if (cli_str_starts_with((char*)vector_get(cli->commands, i), to_complete)) {
            hits[nb_hits++] = i;
        }
    }

    if (nb_hits == 1) {
        // Single hit. Go ahead and complete the command.
        const char* hit_command = (char*)vector_get(cli->commands, hits[0]);
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
                size_t nb_curr_match = cli_str_matching_chars((char*)vector_get(cli->commands, hits[i]),
                                                              (char*)vector_get(cli->commands, hits[j]));
                if (nb_curr_match < nb_common_chars) {
                    nb_common_chars = nb_curr_match;
                    match_idx = hits[i];
                }
            }
        }

        // Get the common chars.
        char matched_chars[nb_common_chars + 1];
        memcpy(matched_chars, (char*)vector_get(cli->commands, match_idx), nb_common_chars);
        matched_chars[nb_common_chars] = '\0';

        // How much of the matched string have already been inputed, and can be ignored?
        size_t ignore = strlen(to_complete);
        sprintf(*completion, "%s", matched_chars + ignore);
    }
}

static void
cli_show_completions(cli_t* cli, const char* substr)
{
    uint8_t nb_cli_commands = vector_length(cli->commands);
    size_t  substr_len = strlen(substr);
    char    to_complete[substr_len + 1];

    // Copy substr and its nullbyte to to_complete.
    memcpy(to_complete, substr, substr_len + 1);

    // Strip newline from to_complete.
    size_t to_complete_len = strlen(to_complete) + 1;
    to_complete[to_complete_len] = '\0';

    // Show which commands were hit.
    printf("\n");
    for (uint8_t i = 0; i < nb_cli_commands; i++) {
        if (cli_str_starts_with((char*)vector_get(cli->commands, i), to_complete)) {
            printf("%s\n", (char*)vector_get(cli->commands, i));
        }
    }
    printf("\n");
    fflush(stdout);
}

static void
cli_print_prompt(cli_t* cli)
{
    printf("%s", cli->prompt_str);
    fflush(stdout);
}

__attribute__((used))
static bool
cli_str_insert_char(const char* input, const char ins_char, const size_t idx, char** output)
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
cli_get_user_input(cli_t* cli)
{
    char   input_buf[MAX_LENGTH_DEBUG_CLI_COMMAND] = {0};
    size_t nb_read   = 0;
    char   prev_char = '\0';

    cli_enable_raw_mode();
    for (;;) {
        char curr_char = '\0';
        size_t read_result = read(STDIN_FILENO, &curr_char, 1);
        if (read_result != 1 || curr_char == '\0') {
            ginger_log(ERROR, "Could not read char from stdin!\n");
            return;
        }

        if (cli_is_arrow_key(curr_char) != 0) {
            // Ignore the arrow keys. Might want to change this to make the user
            // able to manouver the cursor left and right when editing the debug
            // command.
            continue;
        }
        else if (prev_char == '\t' && curr_char == '\t') {
            // Double tab. Show completion options.
            cli_show_completions(cli, input_buf);
            cli_print_prompt(cli);
            printf("%s", input_buf);
            fflush(stdout);
            continue;
        }
        else if (curr_char == '\t') {
            // Do the autocompletion here.
            char* completion = calloc(MAX_LENGTH_DEBUG_CLI_COMMAND, sizeof(char));
            cli_complete_command(cli, input_buf, &completion);
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
                printf("\r%s%s", cli->prompt_str, spaces);
                nb_read--;
                input_buf[nb_read] = '\0';
                printf("\r%s%s", cli->prompt_str, input_buf);
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
    cli_disable_raw_mode();
}

/* --------------------------------------------------- Public ------------------------------------------------------- */

void
cli_add_command(const cli_t* cli, struct cli_cmd command)
{
    vector_append(cli->commands, &command);
}

cli_t*
cli_create(const char* prompt_str)
{
    cli_t* cli = calloc(1, sizeof(cli_t));

    // Set the members.
    cli->prompt_str = prompt_str;
    cli->commands   = vector_create(sizeof(struct cli_cmd));

    // Set the function pointers for the API.
    cli->print_prompt   = cli_print_prompt;
    cli->get_user_input = cli_get_user_input;

    return cli;
}

void
cli_destroy(cli_t* cli)
{
    vector_destroy(cli->commands);
    free(cli);
}
