#ifndef EMU_DEBUG_H
#define EMU_DEBUG_H

#include "../emu/risc_v_emu.h"

// Give the user the ability to show values in the emulator memory, print
// emulator register state or go to the next instruction. If no debug command
// was entered, we execute the last entered command, like GDB does.
void
debug_emu(risc_v_emu_t* emu);

#endif // EMU_DEBUG_H
