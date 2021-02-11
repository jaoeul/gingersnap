Gingersnap shapshot fuzzer
==========================

Project is inspired by Gamozolabs.

This is a work in progress.

# Components

## MMU
The core idea of this project is to restart binaries
quickly with different input. When a specific input
triggers a crash, the fuzzer will note the input and
proceed with the next input.

We will need to be able to detect crashes. The MMU
handles the memory required for the test binary. It
will implement byte-level permission, detectingn when an
illegal address has been accessed.

## CPU emulator
To run the fuzzcaseu deterministically we implement
an CPU emulator. This lets us run any arbitrary binary
compiled for the target emulated architecture fully under
our control.

## Syscalls
A binary that uses syscalls will not work in a pure CPU emulator.
We can bypass this problem either by running the entire OS in the
emulator, or by implementing a function of our own for every
syscalled called by the target binary.

## State reset functionality
To restart the binary quickly in userspace (without having to use
any kind of syscall) we simple revert the state of the CPU and memory
to its initial state, present before execution started.
