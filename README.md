Gingersnap shapshot fuzzer
==========================

This is a work in progress.

The core idea of this project is to implement a fuzzer able to restart
executables in a deterministic and efficient manner, providing new, mutated
input for each test case, with the goal of ultimately crashing the program, or
finding unexpected behavior. When a specific input triggers a crash, the fuzzer
will note the input and proceed with the next input.

# Components

## MMU
We will need to be able to detect crashes. The MMU
handles the memory required for the test binary. It
will implement byte-level permission, detecting when an
illegal address has been accessed.

## CPU emulator
To run the fuzzcases deterministically we implement
an CPU emulator. This lets us run any arbitrary binary
compiled for the target emulated architecture fully under
our control.

## Syscalls
A binary that uses syscalls will not work in a pure CPU emulator.
We can bypass this problem either by running the entire OS in the
emulator, or by implementing a function of our own for every
syscall called by the target binary.

## State reset functionality
To restart the binary quickly in userspace, we simply revert the state of the
CPU and memory to its initial state, present before execution started.

## Risc V
The cpu emulator will implement the riscv 64i instruction set. Target
executables need to be compiled for this architecture, and statically linked.
How to build a target with these properties are described below.

# Usage

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

We are now able to compile source code to riscv64i elfs, the cpu architecture
emulated by gingersnap.


