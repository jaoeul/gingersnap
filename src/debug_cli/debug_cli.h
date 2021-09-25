#ifndef DEBUG_CLI_H
#define DEBUG_CLI_H

#include "../emu/riscv_emu.h"
#include "../shared/cli.h"

typedef struct {
    const rv_emu_t* snapshot;          // The starting point of the fuzzcases.
    uint64_t        fuzz_buf_adr;      // Guest address where the fuzzcases will be injected.
    uint64_t        fuzz_buf_size;     // Size of the buffer which fuzzcases will be injected to.
    bool            snapshot_set;      // If the snapshot has been set.
    bool            fuzz_buf_adr_set;  // If the starting address has been set.
    bool            fuzz_buf_size_set; // If the fuzzing buffer size has been set.
} debug_cli_result_t;

// Give the user the ability to show values in the emulator memory, print
// emulator register state or go to the next instruction. If no debug command
// was entered, we execute the last entered command, like GDB does.
debug_cli_result_t*
debug_cli_run(rv_emu_t* emu, cli_t* cli);

// Create a debugging CLI for the emulator.
cli_t*
debug_cli_create(rv_emu_t* emu);

#endif // DEBUG_CLI_H
