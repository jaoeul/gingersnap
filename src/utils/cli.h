// Implementation of a general purpose CLI.

#ifndef CLI_H
#define CLI_H

#include <stdbool.h>
#include <stdlib.h>

#include "../utils/token_str.h"
#include "../utils/vector.h"

#define MAX_LENGTH_DEBUG_CLI_COMMAND             256
#define MAX_LENGTH_DEBUG_CLI_COMMAND_DESCRIPTION 1024

struct cli_cmd {
    char cmd_str[MAX_LENGTH_DEBUG_CLI_COMMAND];
    char description[MAX_LENGTH_DEBUG_CLI_COMMAND_DESCRIPTION];
};

typedef struct {
    // The CLI API.
    void         (*print_prompt)();
    token_str_t* (*get_command)();
    void         (*free_user_input)(token_str_t* token_str);

    // Takes a command struct and copies the data over to the cli->commands vector.
    // There is no need for the callee to allocate data for the command as this is
    // handled by the vector.
    void  (*add_command)();

    const char*  prompt_str;
    vector_t*    commands;
} cli_t;

cli_t*
cli_create(const char* prompt_str);

void
cli_destroy(cli_t*);

#endif // CLI_H
