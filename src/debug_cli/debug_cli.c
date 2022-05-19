#include "debug_cli.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../emu/emu_generic.h"
#include "../utils/cli.h"
#include "../utils/logger.h"
#include "../utils/print_utils.h"
#include "../utils/vector.h"

#define MAX_NB_BREAKPOINTS 256
#define MAX_LEN_REG_STR    4

// Required amount of characters to display the larges integer supported by an
// 64 bit system.
#define MAX_NB_EXAMINE_ADDRESS 19

static const char size_letters[]              = { 'b', 'h', 'w', 'g' };
static const int  nb_size_letters             = sizeof(size_letters) / sizeof (size_letters[0]);
static const char reg_strs[][MAX_LEN_REG_STR] = { "ra", "sp", "gp", "tp", "t0", "t1", "t2", "fp", "s1", "a0", "a1",
                                                  "a2", "a3", "a4", "a5", "a6", "a7", "s2", "s3", "s4", "s5", "s6",
                                                  "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6", "pc" };
static const int  nb_reg_strs                 = sizeof(reg_strs) / sizeof(reg_strs[0]);
static const char* debug_instructions = "\n"                      \
    "Available CLI commands:\n"                                   \
    " xmem      Examine emulator memory.\n"                       \
    " smem      Search for value in emulator memory.\n"           \
    " ni        Execute next instruction.\n"                      \
    " ir        Show emulator registers.\n"                       \
    " break     Set breakpoint.\n"                                \
    " sbreak    Show all breakpoints.\n"                          \
    " continue  Run emulator until breakpoint or program exit.\n" \
    " snapshot  Take a snapshot of the current emulator state.\n" \
    " adr       Set the address of the target buffer to fuzz.\n"  \
    " length    Set the length of the target buffer to fuzz.\n"   \
    " go        Start the fuzzer.\n"                              \
    " options   Show values of the adjustable options.\n"         \
    " help      Print this help.\n"                               \
    " quit      Quit debugging and exit this program.\n";

static bool
is_number(const char* num, int base)
{
    int i = 0;
    if (base == 10) {
        while (num[i]) {
            // 0 - 9
            if (num[i] < 47 || num[i] > 58) {
                return false;
            }
            i++;
        }
    }
    else if (base == 16) {
        if (strncmp(num, "0x", 2) == 0) {
            i = 2;
        }
        while (num[i]) {
            switch(num[i]) {
                // 0 - 9
                case 48 ... 57:
                    break;
                // A - F
                case 65 ... 70:
                    break;
                // a - f
                case 97 ... 102:
                    break;
                default:
                    return false;
            }
            i++;
        }
    }
    return true;
}

static bool
is_size_letter(const char size_letter)
{
    for (int i = 0; i < nb_size_letters; i++) {
        if (size_letter == size_letters[i]) {
            return true;
        }
    }
    return false;
}

static bool
is_reg_str(const char* reg_str)
{
    for (int i = 0; i < nb_reg_strs; i++) {
        if (strcmp(reg_str, reg_strs[i]) == 0) {
            return true;
        }
    }
    return false;
}

static void
debug_cli_handle_xmem(emu_t* emu, const token_str_t* xmem_args)
{
    char     size_letter = 'w';
    uint64_t range       = 1;
    uint64_t adr         = 0;

    if (xmem_args->nb_tokens == 4) {
        if (!is_number(xmem_args->tokens[3], 16)) {
            printf("\nInvalid address!\n");
            return;
        }
        adr = strtoul(xmem_args->tokens[3], NULL, 16);
        if (strlen(xmem_args->tokens[2]) > 1) {
            printf("\nInvalid size letter!\n");
            return;
        }
        size_letter = xmem_args->tokens[2][0];
        if (!is_number(xmem_args->tokens[1], 10)) {
            printf("\nInvalid range!\n");
            return;
        }
        range = strtoul(xmem_args->tokens[1], NULL, 10);
    }
    else if (xmem_args->nb_tokens == 3) {
        if (!is_number(xmem_args->tokens[2], 16)) {
            printf("\nInvalid address!\n");
            return;
        }
        adr = strtoul(xmem_args->tokens[2], NULL, 16);
        if (strlen(xmem_args->tokens[1]) > 1) { // TODO
            printf("\nInvalid size letter!\n");
            return;
        }
        size_letter = xmem_args->tokens[1][0];
    }
    else if (xmem_args->nb_tokens == 2) {
        if (!is_number(xmem_args->tokens[1], 16)) {
            printf("\nInvalid address!\n");
            return;
        }
        adr = strtoul(xmem_args->tokens[1], NULL, 16);
    }
    else {
        printf("\nInvalid number of args to xmem!\n");
        return;
    }
    mmu_t* mmu = emu->get_mmu(emu);
    mmu->print(mmu, adr, range, size_letter);
}

// Search emulator memory for user specified value.
static void
debug_cli_handle_smem(emu_t* emu, token_str_t* smem_args)
{
    char     size_letter = 'b';
    uint64_t needle      = 0;

    // Three tokens: smem <size_letter> <needle>
    if (smem_args->nb_tokens == 3) {
        if (is_number(smem_args->tokens[2], 16)) {
            needle = strtoul(smem_args->tokens[2], NULL, 16);
        }
        else {
            printf("\nInvalid needle!\n");
            return;
        }
        if (is_size_letter(smem_args->tokens[1][0])) {
            size_letter = smem_args->tokens[1][0];
        }
        else {
            printf("\nInvalid size letter!\n");
            return;
        }
    }
    // Two tokens: smem <needle>
    else if (smem_args->nb_tokens == 2) {
        if (is_number(smem_args->tokens[1], 16)) {
            needle = strtoul(smem_args->tokens[1], NULL, 16);
        }
        else {
            printf("\nInvalid needle!\n");
            return;
        }
    }
    // Invalid command.
    else {
        printf("\nInvalid number of args to smem!\n");
        return;
    }

    mmu_t* mmu = emu->get_mmu(emu);
    vector_t* search_result = mmu->search(mmu, needle, size_letter);
    if (search_result) {
        printf("\n%zu hit(s) of 0x%zx\n", vector_length(search_result), needle);
        for (size_t i = 0; i < search_result->length; i++) {
            printf("%zu: 0x%lx\n", i + 1, *(size_t*)vector_get(search_result, i));
        }
        vector_destroy(search_result);
    }
    else {
        printf("\nDid not find 0x%zx in emulator memory\n", needle);
    }
}

static void
debug_cli_handle_ni(emu_t* emu)
{
    emu->execute(emu);
}

static void
debug_cli_handle_ir(emu_t* emu)
{
    emu->print_regs(emu);
}

static void
debug_cli_handle_break(emu_t* emu, token_str_t* break_args, vector_t* breakpoints)
{
    mmu_t* mmu = emu->get_mmu(emu);

    if (break_args->nb_tokens != 2) {
        printf("\nInvalid number of args to break!\n");
        return;
    }

    size_t break_adr = 0;
    if (is_number(break_args->tokens[1], 16)) {
        break_adr = strtoul(break_args->tokens[1], NULL, 16);
    }

    if (break_adr > mmu->memory_size) {
        printf("\nCould not set breakpoint at 0x%zx as it is outside of emulator memory!\n", break_adr);
        return;
    }

    if ((mmu->permissions[break_adr] & MMU_PERM_EXEC) == 0) {
        printf("\nCould not set breakpoint at 0x%zx! No execute permissions!\n", break_adr);
        return;
    }
    vector_append(breakpoints, &break_adr);
}

static void
debug_cli_handle_sbreak(emu_t* emu, vector_t* breakpoints)
{
    size_t nb_breakpoints = vector_length(breakpoints);
    if (nb_breakpoints == 0) {
        printf("\nNo breakpoints\n");
        return;
    }

    printf("\nBreakpoints:\n");
    for (size_t i = 0; i < nb_breakpoints; i++) {
        printf("%zu\t0x%zx\n", i, *(size_t*)vector_get(breakpoints, i));
    }
}

static void
debug_cli_handle_watch(emu_t* emu, token_str_t* watch_args, vector_t* watchpoints)
{
    if (watch_args->nb_tokens != 2) {
        printf("\nInvalid number of args to watch!\n");
        return;
    }

    if (!is_reg_str(watch_args->tokens[1])) {
        printf("\nInvalid register!\n");
        return;
    }
    vector_append(watchpoints, watch_args->tokens[1]);
}

static void
debug_cli_handle_swatch(emu_t* emu, vector_t* watchpoints)
{
    size_t nb_watchpoints = vector_length(watchpoints);
    if (nb_watchpoints == 0) {
        printf("\nNo watchpoints\n");
        return;
    }

    printf("\nWatchpoints:\n");
    for (size_t i = 0; i < nb_watchpoints; i++) {
        char* curr_watchpoint = vector_get(watchpoints, i);
        printf("%zu\t%s\n", i, curr_watchpoint);
    }
}

// TODO: Break on register watchpoints.
static void
debug_cli_handle_continue(emu_t* emu, vector_t* breakpoints)
{
    for (;;) {
        emu->execute(emu);
        for (size_t i = 0; i < vector_length(breakpoints); i++) {
            uint64_t curr_pc = emu->get_pc(emu);

            if (curr_pc == *(uint64_t*)vector_get(breakpoints, i)) {
                printf("\nHit breakpoint %zu\t0x%zx\n", i, curr_pc);
                return;
            }
        }
    }
}

static void
debug_cli_handle_snapshot(debug_cli_result_t* res, emu_t* snapshot)
{
    res->snapshot     = snapshot;
    res->snapshot_set = true;
}

static void
debug_cli_handle_adr(debug_cli_result_t* res, token_str_t* adr_args)
{
    if (adr_args->nb_tokens != 2) {
        printf("\nInvalid number of args to adr!\n");
        return;
    }

    if (!is_number(adr_args->tokens[1], 16)) {
        printf("\nInvalid address!\n");
        return;
    }
    res->fuzz_buf_adr     = strtoul(adr_args->tokens[1], NULL, 16);
    res->fuzz_buf_adr_set = true;
}

static void
debug_cli_handle_length(debug_cli_result_t* res, token_str_t* length_args)
{
    if (length_args->nb_tokens != 2) {
        printf("\nInvalid number of args to length!\n");
        return;
    }

    if (!is_number(length_args->tokens[1], 10)) {
        printf("\nInvalid length!\n");
        return;
    }
    res->fuzz_buf_size     = strtoul(length_args->tokens[1], NULL, 10);
    res->fuzz_buf_size_set = true;
}

static debug_cli_result_t*
debug_cli_handle_go(debug_cli_result_t* res)
{
    printf("\n");
    return res;
}

static void
debug_cli_handle_options(debug_cli_result_t* res)
{
    if (res->fuzz_buf_adr_set) {
        printf("\nTarget buffer address: 0x%lx", res->fuzz_buf_adr);
    }
    else {
        printf("\nFuzz input injection address not set.");
    }

    if (res->fuzz_buf_size_set) {
        printf("\nTarget buffer length:  %lu", res->fuzz_buf_size);
    }
    else {
        printf("\nFuzz input injection buffer size not set.");
    }

    if (res->snapshot_set) {
        printf("\nClean emulator snapshot:");

        switch (global_config_get_arch()) {
            case ENUM_SUPPORTED_ARCHS_RISCV64I:
                {
                emu_t* snapshot = res->snapshot;
                snapshot->print_regs(res->snapshot);
                break;
                }
        }
    }
    else {
        printf("\nNo snapshot taken.");
    }
    printf("\n");
}

static void
debug_cli_handle_help(cli_t* cli, token_str_t* help_args)
{
    if (help_args->nb_tokens == 1) {
        printf("%s", debug_instructions);
        return;
    }
    else if (help_args->nb_tokens > 2) {
        printf("\nInvalid number of args to help!\n");
        return;
    }

    for (int i = 0; i < vector_length(cli->commands); i++) {
        struct cli_cmd* cmd = vector_get(cli->commands, i);
        if (strcmp(cmd->cmd_str, help_args->tokens[1]) == 0) {
            printf("\n%s", cmd->description);
            return;
        }
    }
    printf("\nNo help for '%s' found.\n", help_args->tokens[1]);
}

static void
debug_cli_handle_quit(void)
{
    printf("\nExiting...\n");
    exit(0);
}

cli_t*
debug_cli_create(void)
{
    struct cli_cmd debug_cli_commands[] = {
        {
            .cmd_str = "xmem",
            .description = "Examine emulator memory.\n"                                        \
                           "Examples:\n"                                                       \
                           "xmem 10 b 0x100c8 // Display 10 bytes from 0x100c8 and up.\n"      \
                           "xmem 10 h 0x100c8 // Display 10 half words from 0x100c8 and up.\n" \
                           "xmem 5 w 0x0      // Display 10 words from 0x0 and up.\n"          \
                           "x g 0x1           // Display 1 double word at address 0x1.\n"      \
        },
        {
            .cmd_str = "smem",
            .description = "Search for sequence of bytes in guest memory.\n"                      \
                           "Examples:\n"                                                          \
                           "smem b 0xff               // Byte aligned search of '0xff'.\n"        \
                           "smem h 0xabcd             // Half word aligned search of '0xabcd'.\n" \
                           "smem w 0xcafebabe         // Word aligned search of '0xcafebabe'.\n"  \
                           "smem g 0xdeadc0dedeadbeef // Double word aligned search of '0xdeadc0dedeadbeef'.\n"
                           "sm 0xff                   // Byte aligned search of '0xff'.\n"        \
        },
        {
            .cmd_str = "ni",
            .description = "Execute next instruction.\n"
        },
        {
            .cmd_str = "ir",
            .description = "Show emulator registers.\n"
        },
        {
            .cmd_str = "break",
            .description = "Set breakpoint.\n" \
                           "Example: break 0x10218\n"
        },
        {
            .cmd_str = "watch",
            .description = "Set register watchpoint.\n" \
                           "Example: watch sp\n"
        },
        {
            .cmd_str = "sbreak",
            .description = "Show all breakpoints.\n"
        },
        {
            .cmd_str = "swatch",
            .description = "Show all watchpoints.\n"
        },
        {
            .cmd_str = "continue",
            .description = "Run emulator until breakpoint or program exit.\n"
        },
        {
            .cmd_str = "snapshot",
            .description = "Take a snapshot.\n"
        },
        {
            .cmd_str = "adr",
            .description = "Set the address in guest memory where fuzzed input will be injected.\n" \
                           "Example: adr 0x1ffea8\n"

        },
        {
            .cmd_str = "length",
            .description = "Set the fuzzer injection input length.\n" \
                           "Example: length 4\n"
        },
        {
            .cmd_str = "go",
            .description = "Try to start the fuzzer.\n"
        },
        {
            .cmd_str = "options",
            .description = "Show values of the adjustable options.\n"
        },
        {
            .cmd_str     = "help",
            .description = "Displays help text of a command.\n" \
                           "Example: help xmem\n"

        },
        {
            .cmd_str = "quit",
            .description = "Quit debugging and exit this program.\n"
        }
    };

    const char* prompt_str = "(gingersnap) ";
    cli_t* debug_cli = cli_create(prompt_str);

    for (int i = 0; i < sizeof(debug_cli_commands) / sizeof(debug_cli_commands[0]); i++) {
        debug_cli->add_command(debug_cli, debug_cli_commands[i]);
    }

    return debug_cli;
}

// Run the debug CLI. If all result values are set, return a `debug_cli_result_t*`.
debug_cli_result_t*
debug_cli_run(emu_t* emu, cli_t* cli)
{
    static token_str_t* prev_cli_tokens; // Static variables are zero initialized, at program start.
    vector_t*           breakpoints = vector_create(sizeof(uint64_t));
    vector_t*           watchpoints = vector_create(sizeof(MAX_LEN_REG_STR));
    debug_cli_result_t* cli_result  = calloc(1, sizeof(debug_cli_result_t));

    for (;;) {
        printf("\n");
        cli->print_prompt(cli);

        // Can only return valid command tokens. Unknown input is already
        // handled by the CLI by this point.
        token_str_t* cli_tokens = cli->get_command(cli);

        // We got no new command but enter was pressed, use the last command instead.
        if (!cli_tokens) {
            // Use the last command.
            if (prev_cli_tokens) {
                cli_tokens = token_str_copy(prev_cli_tokens);

                // Clean up earlier saved command tokens.
                token_str_destroy(prev_cli_tokens);
            }
            // If we got no new command, and we have no previous command,
            // simply skip this iteration.
            else {
                continue;
            }
        }
        char* command_str = cli_tokens->tokens[0];

        if (strncmp(command_str, "xmem", 4) == 0) {
            debug_cli_handle_xmem(emu, cli_tokens);
        }
        else if (strncmp(command_str, "smem", 4) == 0) {
            debug_cli_handle_smem(emu, cli_tokens);
        }
        else if (strncmp(command_str, "ni", 2) == 0) {
            debug_cli_handle_ni(emu);
        }
        else if (strncmp(command_str, "ir", 2) == 0) {
            debug_cli_handle_ir(emu);
        }
        else if (strncmp(command_str, "break", 5) == 0) {
            debug_cli_handle_break(emu, cli_tokens, breakpoints);
        }
        else if (strncmp(command_str, "sbreak", 5) == 0) {
            debug_cli_handle_sbreak(emu, breakpoints);
        }
        else if (strncmp(command_str, "watch", 5) == 0) {
            debug_cli_handle_watch(emu, cli_tokens, watchpoints);
        }
        else if (strncmp(command_str, "swatch", 5) == 0) {
            debug_cli_handle_swatch(emu, watchpoints);
        }
        else if (strncmp(command_str, "continue", 8) == 0) {
            debug_cli_handle_continue(emu, breakpoints);
        }
        else if (strncmp(command_str, "snapshot", 8) == 0) {
            debug_cli_handle_snapshot(cli_result, emu);
        }
        else if (strncmp(command_str, "adr", 3) == 0) {
            debug_cli_handle_adr(cli_result, cli_tokens);
        }
        else if (strncmp(command_str, "length", 6) == 0) {
            debug_cli_handle_length(cli_result, cli_tokens);
        }
        else if (strncmp(command_str, "go", 2) == 0) {
            return debug_cli_handle_go(cli_result);
        }
        else if (strncmp(command_str, "options", 7) == 0) {
            debug_cli_handle_options(cli_result);
        }
        else if (strncmp(command_str, "help", 4) == 0) {
            debug_cli_handle_help(cli, cli_tokens);
        }
        else if (strncmp(command_str, "quit", 4) == 0) {
            debug_cli_handle_quit();
        }

        // Free the heap allocated user input string.
        cli->free_user_input(cli_tokens);
    }
    vector_destroy(breakpoints);
    vector_destroy(watchpoints);
}
