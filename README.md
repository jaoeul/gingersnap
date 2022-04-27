Gingersnap shapshot fuzzer
==========================

Disclaimer: This is a work in progress.

The core idea of this project is to provide better harnessing and more efficient fuzzing
of executables than that of traditional fuzzers, like AFL. Running the target executable
in an emulator gives us full control, and by implementing the mmu ourselves, we gain
bytewise granuality of memory permissions, allowing us to detect read or writes only
one byte off.

# Building
```
mkdir build
cd build
cmake ..
make -j
```

# Usage
```
Usage:
gingersnap -t "<target> <arg_1> ... <arg_n>" -c <corpus_dir>

Flags:
   -t     Target program and arguments.
   -c     Path to directory with corpus files.
   -j     Number of cores to use for fuzzing. Defauts to all active cores on the
          system.
   -p     Progress directory, where inputs which generated new coverage will be
          stored. Defaults to `./progress`.
   -v     Print stdout from emulators to stdout.
   -n     No coverage. Do not track coverage.
   -h     Print this help text.

Available pre-fuzzing commands:
   xmem       Examine emulator memory.
   smem       Search for sequence of bytes in guest memory.
   ni         Execute next instruction.
   ir         Show emulator registers.
   break      Set breakpoint.
   watch      Set register watchpoint.
   sbreak     Show all breakpoints.
   swatch     Show all watchpoints.
   continue   Run emulator until breakpoint or program exit.
   snapshot   Take a snapshot.
   adr        Set the address in guest memory where fuzzed input will be injected.
   length     Set the fuzzer injection input length.
   go         Try to start the fuzzer.
   options    Show values of the adjustable options.
   help       Displays help text of a command.
   quit       Quit debugging and exit this program.

Run `help <command>` in gingersnap for further details and examples of command
usage.

Typical usage example:
   Step 1: Run the emulator to desireable pre-fuzzing state. This can be done by
           single-stepping or by setting a breakpoint and continuing exection.
       (gingersnap) ni
       (gingersnap) break <guest_address>
       (gingersnap) continue

   Step 2: Set the address and length of the buffer in guest memory where
           fuzzcases will be injected. This is a required step.
       (gingersnap) adr <guest_address>
       (gingersnap) len <length>

   Step 3: Start fuzzing:
       (gingersnap) go
```

# Components

## CPU emulator
To run the fuzzcases deterministically we implement
an CPU emulator. This lets us run any arbitrary binary
compiled for the emulated architecture fully under
our control. The riscv 64i CPU architecture is the
platform of choice for this project. The emulator
is designed with multi-threading and affinity in mind,
allowing us to run one emulator per cpu core.

## MMU
The MMU (memory management unit) handles the memory required for the target executable.
It implements byte-level permission, detecting when an illegal address has been accessed.

## Syscalls
A binary that uses syscalls will not work in a pure CPU emulator.
We can bypass this problem either by running the entire OS in the
emulator, or by implementing a function of our own for every
syscall called by the executable under test.

## Snapshots
A snapshot consists of cpu and mmu state.

## State reset functionality
To restart the target quickly in userspace, we simply reset the state of the
CPU and mmu to its initial pre-fuzzed snapshot. This allows for great
performance as resets scale linearly with number of cpu cores.

## Risc V
The cpu emulator will implement the riscv 64i instruction set. Target
executables need to be statically linked and compiled for this architecture.
How to build a target with these properties are described below.

# Building a target
Gingersnap uses a cpu emulator, and does its work on machine instructions.
This means that you do not need the source code for your target. However,
if you would like to build a target for testing purposes, the below section
describes how.

## Setup

### Build a riscv toolchain
Before we do anything else, a riscv toolchain is needed. The following sequence
of commands will build tools including readelf and objectdump, e.g. more than
only the essential crosscompiler, but they will come in handy later, so we might
as well build them.

The version of gcc we get by running the following commands will be able to
compile C code to a 64 bit riscvi exacutable. This is a tiny instruction set,
consisting of only 40 different instructions, which is easy to emulate.

```bash
git clone https://github.com/riscv/riscv-gnu-toolchain.git
cd riscv-gnu-toolchain
./configure --prefix=<prefix_path> --with-arch=rv64
make -j $(nproc)
```

The resulting compiler should be called `riscv64-unknown-linux-gnu-gcc`.

Create a sample C program and compile it to a statically linked riscv 64
bit elf

### Compiling source code to riscv 64i elf

```bash
riscv64-unknown-linux-gnu-gcc -static -march=rv64i ./opt/riscv/bin/test.c -o <name_of_exe>
```

### Run the riscv executable
```bash
sudo apt-get install qemu-user
qemu-riscv64 <name_of_exe>
```

## Debugging and singlestepping the target executable

Useful to find emulator bugs. Single step the executable in gdb and compare the
how the registers change values in gdb and compare it to how they change in the
emulator.

```bash
qemu-riscv64 -g 1234 <target_binary>

# Switch to another terminal
riscv64-unknown-linux-gnu-gdb
```

```gdb
riscv64-unknown-linux-gnu-gdb <target_binary>
target remote localhost:1234
break *<program_entry_point_address>
run
```

## Browse the instructions of the target
Note that the `no-aliases` option shows only the canonical instructions, rather than
pseudoinstructions.

```bash
riscv_build_toolchain/bin/riscv64-unknown-elf-objdump -M intel,no-aliases -D ./target | less
```
