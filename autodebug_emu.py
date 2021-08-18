#! /usr/bin/python3

# This script steps through a target executable using both gingersnap and gdb.
# If cpu registers differ at any point, an error is thrown and execution is halted.
#
# We use qemu to run the target executable, since it is compiled for risc v.
# We provide qemu with the -g option, to stop and wait for a debugger to connect.
# We then connect to qemu with gdb.
# Now we can automate singlestepping and comparing registers! \o/

# We use pwntools to connect to emulator and gdb's stdin/stdout.
from pwn import *
from subprocess import Popen, PIPE, STDOUT

import os

# The registers we care about. Could omit the stack pointer(sp) here.
cmp_regs = [ "ra", "sp", "gp", "tp", "t0", "t1", "t2", "fp", "s1", "a0", "a1",
             "a2", "a3", "a4", "a5", "a6", "a7", "s2", "s3", "s4", "s5", "s6",
             "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6", "pc"]

def kill_proc(proc_name):
    proc = Popen(['ps', '-A'], stdout=PIPE)
    output, error = proc.communicate()
    for line in output.splitlines():
        if proc_name in str(line):
            pid = int(line.split(None, 1)[0])
            os.kill(pid, 9)

def gdb_get_regs(gdb_proc) -> list:
    gdb_proc.sendline("info registers")
    gdb_proc.recvuntil("(gdb) ")
    gdb_regs = []
    for reg in cmp_regs:
        gdb_regs.append(gdb_proc.recvline_contains(reg).decode().split()[:2])
    return gdb_regs

def gdb_next_instruction(gdb_proc):
    gdb_proc.sendline("ni")
    gdb_proc.recvuntil("(gdb) ")

def gdb_attach_to_qemu():
    gdb_args = ["./riscv_build_toolchain/bin/riscv64-unknown-elf-gdb"]
    try:
        gdb_proc = process(gdb_args)
    except:
        exit(1)
    sleep(1)
    gdb_proc.sendline("c") # Continue without paging
    gdb_proc.recvuntil("(gdb) ")
    gdb_proc.sendline("set pagination off")
    gdb_proc.sendline("target remote :1234")
    print("GDB connecting to remote qemu process.")
    sleep(1)
    gdb_proc.recvuntil("(gdb) ")
    return gdb_proc

def emu_get_regs(emu_proc) -> list:
    emu_proc.sendline("info registers")
    emu_proc.recvuntil("(debug) ")
    emu_regs = []
    for reg in cmp_regs:
        emu_regs.append(emu_proc.recvline_contains(reg).decode().split()[:2])
    return emu_regs

def emu_next_instruction(emu_proc):
    emu_proc.sendline("next instruction")
    emu_proc.recvuntil("(debug) ")

def compare_regs(emu_regs, gdb_regs, emu_regs_prev, gdb_regs_prev) -> list:
    if len(emu_regs) != len(gdb_regs):
        print("Unequal number of registers")
        exit(1)

    diff = [("emu", "gdb")]
    for i in range(0, len(emu_regs)):
        if emu_regs[i] != gdb_regs[i]:
            tup = (emu_regs[i], gdb_regs[i])
            diff.append(tup)
    return diff

if __name__ == "__main__":

    # General setup.
    context.log_level = "error"#"DEBUG"

    print("Killing stray processes.")
    kill_proc("qemu-riscv64")
    kill_proc("riscv64-unknown-elf-gdb")

    print("Spawning emulator process.")
    emu_args = ["./release_gingersnap", "./target"]
    emu_proc = process(emu_args)
    emu_proc.recvuntil("(debug) ")

    print("Spawning qemu process.")
    qemu_args = ["qemu-riscv64", "-g", "1234", "./target"]
    qemu_proc = process(qemu_args)
    sleep(1)

    print("Attaching gdb to qemu process.")
    gdb_proc = gdb_attach_to_qemu()

    gdb_regs_prev = []
    emu_regs_prev = []
    last_diff     = [()] # List of tuples.
    while 1:
        gdb_regs = gdb_get_regs(gdb_proc)
        emu_regs = emu_get_regs(emu_proc)

        diff = compare_regs(emu_regs, gdb_regs, emu_regs_prev, gdb_regs_prev)

        # No need to compare first and second instructions.
        if diff != last_diff and len(emu_regs_prev) != 0:
            print(f"\nemu prev pc: {emu_regs_prev[-1]}, gdb prev pc: {gdb_regs_prev[-1]}")
            for _ in diff:
                print(_)
            input()

        emu_next_instruction(emu_proc)
        gdb_next_instruction(gdb_proc)

        gdb_regs_prev = gdb_regs
        emu_regs_prev = emu_regs
        last_diff     = diff

    gdb_proc.interactive()
    emu_proc.interactive()
    qemu_proc.interactive()
