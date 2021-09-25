#include "cli.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../shared/logger.h"
#include "../shared/vector.h"

#define NO_MOVE_ARROW 0

enum ESCAPE_SEQUENCE {
    ARROW_UP    = NO_MOVE_ARROW,
    ARROW_DOWN  = NO_MOVE_ARROW,
    ARROW_RIGHT = 1,
    ARROW_LEFT  = -1
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

    // Apply new terminal attributes.
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// The only escape sequence we care about are arrow key presses.
// Entering an arrow key sends 3 chars. Esc(27), [(91), and either one of the values
// in the below switch case.
static int
cli_handle_escape_sequence(const char esc_seq1)
{
    char esc_seq2 = getc(stdin);
    char esc_seq3 = getc(stdin);
    if (esc_seq1 == 27) {
        if (esc_seq2 == 91) {
            switch (esc_seq3) {
                case 65:
                    return ARROW_UP;
                case 66:
                    return ARROW_DOWN;
                case 67:
                    return ARROW_RIGHT;
                case 68:
                    return ARROW_LEFT;
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

    if (string_len >= substr_len) {
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

// Return true if exactly one matching command was found and completed,
// otherwise return false.
// Used for tab completion and to execute the correct command if only a substring is given.
static bool
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
        const struct cli_cmd* curr_cmd = vector_get(cli->commands, i);
        if (cli_str_starts_with(curr_cmd->cmd_str, to_complete)) {
            hits[nb_hits++] = i;
        }
    }

    // Single hit. Go ahead and complete the command.
    if (nb_hits == 1) {
        const struct cli_cmd* hit_command = vector_get(cli->commands, hits[0]);
        const size_t matched_str_len = strspn(hit_command->cmd_str, to_complete);
        sprintf(*completion, "%s", hit_command->cmd_str + matched_str_len);
        return true;
    }
    // Multiple hits. Complete command as far as possible.
    else if (nb_hits > 1) {
        size_t  nb_common_chars = MAX_LENGTH_DEBUG_CLI_COMMAND;
        uint8_t match_idx       = 0;

        // TODO: See if we can lower time complexity here.
        for (uint8_t i = 0; i < nb_hits; i++) {
            for (uint8_t j = 0; j < nb_hits; j++) {
                if (i == j) {
                    continue;
                }
                const struct cli_cmd* cmd_i = vector_get(cli->commands, hits[i]);
                const struct cli_cmd* cmd_j = vector_get(cli->commands, hits[j]);
                const size_t nb_curr_match  = cli_str_matching_chars(cmd_i->cmd_str, cmd_j->cmd_str);
                if (nb_curr_match < nb_common_chars) {
                    nb_common_chars = nb_curr_match;
                    match_idx = hits[i];
                }
            }
        }

        // Get the common chars.
        char matched_chars[nb_common_chars + 1];
        struct cli_cmd* matched_cmd = vector_get(cli->commands, match_idx);
        memcpy(matched_chars, matched_cmd->cmd_str, nb_common_chars);
        matched_chars[nb_common_chars] = '\0';

        // How much of the matched string have already been inputed, and can be ignored?
        size_t ignore = strlen(to_complete);
        sprintf(*completion, "%s", matched_chars + ignore);
    }
    return false;
}

static void
cli_show_completions(cli_t* cli, const char* substr)
{
    const uint8_t nb_cli_commands = vector_length(cli->commands);

    // Show which commands were hit.
    printf("\n");
    for (uint8_t i = 0; i < nb_cli_commands; i++) {
        const struct cli_cmd* curr_cmd = vector_get(cli->commands, i);
        if (cli_str_starts_with(curr_cmd->cmd_str, substr)) {
            printf("%s\n", curr_cmd->cmd_str);
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

// TODO: Use this when CLI cursor movement is supported.
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

// TODO: Use this to move the CLI cursor on arrow keypress.
//       Moves the terminal cursor to coordinate x, y.
__attribute__((used))
static void
cli_gotoxy(const size_t x, const size_t y)
{
#ifdef __linux__
    cli_disable_raw_mode();
    printf("\033[%zu;%zuH", x, y);
    fflush(stdout);
    cli_enable_raw_mode();
#else
    ginger_log(ERROR, "%s() implementation missing!\n", __func__);
    fflush(stdout);
#endif
}

// If substr is found at exactly one place in the vector, return the index.
// Else, return -1.
static int
cli_str_search_exact_match(vector_t* commands, const char* substr)
{
    const uint8_t nb_cli_commands = vector_length(commands);
    uint8_t       nb_hits         = 0;
    size_t        hit_idx         = 0;

    // Note how many strings were hit.
    for (uint8_t i = 0; i < nb_cli_commands; i++) {
        const struct cli_cmd* curr_cmd = vector_get(commands, i);
        if (cli_str_starts_with(curr_cmd->cmd_str, substr)) {
            hit_idx = i;
            nb_hits++;
        }
    }

    if (nb_hits == 1) {
        return hit_idx;
    }
    else {
        return -1;
    }
}

// Returns heap allocated user input string. Support command autocompletion.
static char*
cli_get_command(cli_t* cli)
{
    char   input_buf[MAX_LENGTH_DEBUG_CLI_COMMAND] = {0};
    size_t nb_read   = 0; // Increments on character input or autocompletion.
    char   prev_char = '\0';

    // Raw mode gives us more control over the terminal, enabling us to detect
    // kchar2eypresses before a newline registerd.
    cli_enable_raw_mode();

    // Run until user is finished inputting characters.
    for (;;) {
        char curr_char = '\0';
        size_t read_result = read(STDIN_FILENO, &curr_char, 1);
        if (read_result != 1 || curr_char == '\0') {
            ginger_log(ERROR, "Could not read char from stdin!\n");
            return NULL;
        }

        // Debug.
        //printf("%d\n", curr_char);

        // Escape, expect an following escape sequence.
        if (curr_char == '\033') {
            const int arrow_action = cli_handle_escape_sequence(curr_char);
            (void)arrow_action;
            // TODO: Fix cursor positioning on arrow key presses.
            //       Use a curr_pos variable instead of nb_read.
            /*
            if (arrow_action == NO_MOVE_ARROW) {
                continue;
            }
            // If we got an arrow key press to the right or to the left, we
            // update the curr_pos variable accordingly.
            // No need to manouver the cursor if there is no input string.
            if (nb_read == 0) {
                continue;
            }
            // Do not move cursor to the right if it is positioned at the end
            // of the string.
            if (curr_pos == nb_read && arrow_action == ARROW_RIGHT) {
                continue;
            }
            // Do not move cursor to the left if it is positioned at the start
            // of the string.
            if (curr_pos == 0 && arrow_action == ARROW_LEFT) {
                continue;
            }
            curr_pos += arrow_action;
            cli_gotoxy(1, curr_pos);
            continue;
            */
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

            // Add the completion to input_buf.
            const size_t comp_len = strlen(completion);
            strcat(input_buf, completion);
            nb_read += comp_len;
            free(completion);
        }
        else if (curr_char == 10) {
            // Got enter. Consider the user input complete.
            if (nb_read == 0) {
                cli_disable_raw_mode();
                return NULL;
            }
            const int match = cli_str_search_exact_match(cli->commands, input_buf);
            if (match != -1) {
                char* completion = calloc(MAX_LENGTH_DEBUG_CLI_COMMAND, sizeof(char));
                cli_complete_command(cli, input_buf, &completion);

                // Add the completion to input_buf.
                const size_t comp_len = strlen(completion);
                strcat(input_buf, completion);
                nb_read += comp_len;
                free(completion);

                // Return the completed command a heap allocated string.
                input_buf[nb_read] = '\0';
                char* user_input = calloc(nb_read, sizeof(char));
                memcpy(user_input, input_buf, nb_read);
                cli_disable_raw_mode();
                return user_input;
            }
            else {
                printf("\nCommand not found!\n");
                fflush(stdout);
            }
        }
        else if (curr_char == 127) {
            // Handle backspace.
            if (nb_read > 0) {
                // Remove the last char by printing a space in its place, and
                // then setting the character to '\0'.
                input_buf[nb_read - 1] = ' ';
                printf("\r%s%s", cli->prompt_str, input_buf);
                input_buf[nb_read - 1] = '\0';
                nb_read--;
            }
        }
        else if (curr_char >= 32 || curr_char <= 127) {
            if (nb_read >= MAX_LENGTH_DEBUG_CLI_COMMAND - 1) {
                continue;
            }
            // Valid printable character.
            input_buf[nb_read++] = curr_char;
        }

        // Print the current user input string.
        printf("\r%s%s", cli->prompt_str, input_buf);
        fflush(stdout);

        // Save the read char in order to detect double tab.
        prev_char = curr_char;
    }
    cli_disable_raw_mode();
}

// Frees the heap allocated user input string.
static void
cli_free_user_input(char* user_input)
{
    free(user_input);
}


static void
cli_add_command(const cli_t* cli, struct cli_cmd command)
{
    vector_append(cli->commands, &command);
}

/*
static bool
cli_execute_command(const cli_t* cli, const char* command, void* arg)
{
    const int match = cli_str_search_exact_match(cli->commands, command);
    if (match == -1) {
        printf("\nCommand %s not found!\n", command);
        return false;
    }

    // Execute the given command.
    const struct cli_cmd* target_cmd = vector_get(cli->commands, match);
    target_cmd->cmd_fn(arg);
    return true;
}
*/

/* --------------------------------------------------- Public ------------------------------------------------------- */

cli_t*
cli_create(const char* prompt_str)
{
    cli_t* cli = calloc(1, sizeof(cli_t));

    // Set the members.
    cli->prompt_str = prompt_str;
    cli->commands   = vector_create(sizeof(struct cli_cmd));

    // Set the function pointers for the API.
    cli->print_prompt    = cli_print_prompt;
    cli->get_command     = cli_get_command;
    cli->free_user_input = cli_free_user_input;
    cli->add_command     = cli_add_command;

    return cli;
}

void
cli_destroy(cli_t* cli)
{
    vector_destroy(cli->commands);
    free(cli);
}
