#ifndef DEBUG_CLI_H
#define DEBUG_CLI_H

#include "../emu/emu_generic.h"
#include "../main/config.h"
#include "../utils/cli.h"

typedef struct {
    emu_t*                 snapshot;          // The starting point of the fuzzcases.
    enum_supported_archs_t arch;              // The arch to cast the snapshot to.
    uint64_t               fuzz_buf_adr;      // Guest address where the fuzzcases will be injected.
    uint64_t               fuzz_buf_size;     // Size of the buffer which fuzzcases will be injected to.
    bool                   snapshot_set;      // If the snapshot has been set.
    bool                   fuzz_buf_adr_set;  // If the starting address has been set.
    bool                   fuzz_buf_size_set; // If the fuzzing buffer size has been set.
} debug_cli_result_t;

// Give the user the ability to show values in the emulator memory, print
// emulator register state or go to the next instruction. If no debug command
// was entered, we execute the last entered command, like GDB does.
debug_cli_result_t*
debug_cli_run(emu_t* emu, cli_t* cli);

cli_t*
debug_cli_create(void);

#endif // DEBUG_CLI_H
