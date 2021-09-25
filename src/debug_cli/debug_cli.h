#ifndef DEBUG_CLI_H
#define DEBUG_CLI_H

#include "../emu/riscv_emu.h"
#include "../shared/cli.h"

// Give the user the ability to show values in the emulator memory, print
// emulator register state or go to the next instruction. If no debug command
// was entered, we execute the last entered command, like GDB does.
rv_emu_t*
debug_cli_run(rv_emu_t* emu, cli_t* cli);

// Create a debugging CLI for the emulator.
cli_t*
debug_cli_create(rv_emu_t* emu);

#endif // DEBUG_CLI_H
