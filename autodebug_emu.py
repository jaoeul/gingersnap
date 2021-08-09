#! /usr/bin/python3

# This script steps through a target executable using both gingersnap and gdb.
# If cpu registers differ at any point, an error is thrown and execution is halted.

# We use pwntools to connect to emulator and gdb's stdin/stdout.
from pwn import *
from subprocess import Popen, PIPE, STDOUT

import os

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
    gdb_regs.append(gdb_proc.recvline_contains("ra").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("sp").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("gp").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("tp").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("t0").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("t1").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("t2").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("fp").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("s1").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("a0").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("a1").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("a2").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("a3").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("a4").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("a5").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("a6").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("a7").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("s2").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("s3").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("s4").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("s5").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("s6").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("s7").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("s8").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("s9").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("s10").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("s11").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("t3").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("t4").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("t5").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("t6").decode().split()[:2])
    gdb_regs.append(gdb_proc.recvline_contains("pc").decode().split()[:2])
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
    emu_regs = []
    emu_regs.append(emu_proc.recvline_contains("ra").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("sp").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("gp").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("tp").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("t0").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("t1").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("t2").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("fp").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("s1").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("a0").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("a1").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("a2").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("a3").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("a4").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("a5").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("a6").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("a7").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("s2").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("s3").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("s4").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("s5").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("s6").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("s7").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("s8").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("s9").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("s10").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("s11").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("t3").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("t4").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("t5").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("t6").decode().split()[:2])
    emu_regs.append(emu_proc.recvline_contains("pc").decode().split()[:2])
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
