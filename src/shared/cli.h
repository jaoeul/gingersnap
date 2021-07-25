// Implementation of a general purpose CLI.

#ifndef CLI_H
#define CLI_H

#include <stdlib.h>

#include "../shared/vector.h"

#define MAX_LENGTH_DEBUG_CLI_COMMAND             64
#define MAX_LENGTH_DEBUG_CLI_COMMAND_DESCRIPTION 1024

struct cli_cmd {
    char cmd_str[MAX_LENGTH_DEBUG_CLI_COMMAND];
    char description[MAX_LENGTH_DEBUG_CLI_COMMAND_DESCRIPTION];

    // Function called when command is executed.
    void (*cmd_fn)();
};

typedef struct {
    // The CLI API.
    void (*print_prompt)();
    void (*get_user_input)();

    const char*  prompt_str;
    vector_t*    commands;
} cli_t;

void
cli_add_command(const cli_t* cli, const struct cli_cmd command);

cli_t*
cli_create(const char* prompt_str);

void
cli_destroy(cli_t*);

#endif // CLI_H
