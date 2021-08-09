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
             "s7", "s8", "s9", "s1", "s1", "t3", "t4", "t5", "t6", "pc"]

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

def compare_regs(emu_regs, gdb_regs) -> bool:
    if len(emu_regs) != len(gdb_regs):
        print("Unequal number of registers")
        return False

    for i in range(0, len(emu_regs)):
        if emu_regs[i] != gdb_regs[i]:

            # We don't care if the stack pointer differs.
            if "sp" in emu_regs[i][0] and "sp" in gdb_regs[i][0]:
                continue
            print("regs not equal")
            print(f"emu pc {emu_regs[-1]}, gdb pc {gdb_regs[-1]}")
            print(f"emu reg {emu_regs[i]}, gdb reg {gdb_regs[i]}")
            return False
    return True

if __name__ == "__main__":

    # General setup.
    context.binary    = "./debug_gingersnap"
    context.os        = "linux"
    context.arch      = "amd64"
    context.terminal  = ["tmux", "split", "-h"]
    context.log_level = "error"#"DEBUG"

    # Kill processes from earlier debugging session if there are any.
    print("Killing existing qemu processes.")
    kill_proc("qemu-riscv64")

    # Spawn emulator process
    print("Spawning emulator process.")
    emu_args = ["./debug_gingersnap", "./target"]
    emu_proc = process(emu_args)
    emu_proc.recvuntil("(debug) ")

    print("Spawning qemu process.")
    qemu_args = ["qemu-riscv64", "-g", "1234", "./target"]
    qemu_proc = process(qemu_args)
    sleep(1)

    print("Attaching gdb to qemu process.")
    gdb_proc = gdb_attach_to_qemu()

    while 1:
        gdb_regs = gdb_get_regs(gdb_proc)
        emu_regs = emu_get_regs(emu_proc)
        if not compare_regs(emu_regs, gdb_regs):
            exit(1)
        emu_next_instruction(emu_proc)
        gdb_next_instruction(gdb_proc)

    gdb_proc.interactive()
    emu_proc.interactive()
    qemu_proc.interactive()
