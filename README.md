Gingersnap shapshot fuzzer
==========================

This is a work in progress.

The core idea of this project is to restart binaries
efficiently and determinintically, providing mutated
input for each run. When a specific input triggers a
crash, the fuzzer will note the input and proceed
with the next input.

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
To restart the binary quickly in userspace, we simply revert the state of the CPU and memory
to its initial state, present before execution started.
