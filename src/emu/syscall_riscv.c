#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "emu_generic.h"
#include "emu_riscv.h"
#include "syscall.h"

#include "../shared/logger.h"
#include "../shared/print_utils.h"

static const bool GUEST_VERBOSE_PRINTS = true;

// Linux kernel 64 stat struct.
struct kernel_stat
{
    unsigned long long st_dev;
    unsigned long long st_ino;
    unsigned int st_mode;
    unsigned int st_nlink;
    unsigned int st_uid;
    unsigned int st_gid;
    unsigned long long st_rdev;
    unsigned long long __pad1;
    long long st_size;
    int st_blksize;
    int __pad2;
    long long st_blocks;
    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
    int __glibc_reserved[2];
};

void
handle_syscall(emu_t* emu, const uint64_t num)
{
    switch(num) {

    // close. Only stdin, stdout and stderr are supported.
    case 57:
        {
            const uint64_t fd = emu->get_reg(emu, RISC_V_REG_A0);
            if (fd > 2) {
                ginger_log(ERROR, "Close syscall is only supported for stdin, stdout and stderr file descriptors!\n");
                ginger_log(ERROR, "fd: %lu\n", fd);
                exit(1);
            }
            // Fake success.
            emu->set_reg(emu, RISC_V_REG_A0, 0);
        }
        break;

    // write. Only stdout and stderr are supported.
    case 64:
        {
            const uint64_t fd            = emu->get_reg(emu, RISC_V_REG_A0);
            const uint64_t buf_guest_adr = emu->get_reg(emu, RISC_V_REG_A1);
            const uint64_t len           = emu->get_reg(emu, RISC_V_REG_A2);

            if (fd != 1 && fd != 2) {
                ginger_log(ERROR, "Write syscall is only supported for stdout and stderr file descriptors!\n");
                ginger_log(ERROR, "fd: %lu\n", fd);
                exit(1);
            }

            if (!GUEST_VERBOSE_PRINTS) {
                // Fake that all bytes were written.
                emu->set_reg(emu, RISC_V_REG_A0, len);
                return;
            }

            uint8_t print_buf[len + 1];
            memset(print_buf, 0, len + 1);

            const uint8_t read_ok = emu->mmu->read(emu->mmu, print_buf, buf_guest_adr, len);
            if (read_ok != 0) {
                emu->exit_reason = EMU_EXIT_REASON_SEGFAULT_READ;
                break;
            }

            ginger_log(DEBUG, "Guest wrote: %s\n", print_buf);
            emu->set_reg(emu, RISC_V_REG_A0, len);
        }
        break;

    // fstat. Write information about a file into emulator memory. We only support stdin, stdout
    // and stderr file descriptors for now.
    case 80:
        {
            const uint64_t fd                = emu->get_reg(emu, RISC_V_REG_A0);
            const uint64_t statbuf_guest_adr = emu->get_reg(emu, RISC_V_REG_A1);

            ginger_log(DEBUG, "fstat syscall\n");
            ginger_log(DEBUG, "fd: %lu\n", fd);
            ginger_log(DEBUG, "statbuf: 0x%lx\n", statbuf_guest_adr);

            struct kernel_stat k_statbuf;
            memset(&k_statbuf, 0, sizeof(k_statbuf));

            // Following magic values were retreived from running fstat on the host OS.
            // stdin
            if (fd == 0) {
                k_statbuf.st_dev          = 0x17;
                k_statbuf.st_ino          = 0x6;
                k_statbuf.st_mode         = 0x2190;
                k_statbuf.st_nlink        = 0x1;
                k_statbuf.st_uid          = 0x3e8;
                k_statbuf.st_gid          = 0x5;
                k_statbuf.st_rdev         = 0x8803;
                k_statbuf.st_size         = 0;
                k_statbuf.st_blocks       = 0;
                k_statbuf.st_blksize      = 1024;
            }
            // stdout
            else if (fd == 1) {
                k_statbuf.st_dev          = 0x17;
                k_statbuf.st_ino          = 0xe;
                k_statbuf.st_mode         = 0x2190;
                k_statbuf.st_nlink        = 0x1;
                k_statbuf.st_uid          = 0x3e8;
                k_statbuf.st_gid          = 0x5;
                k_statbuf.st_rdev         = 0x880b;
                k_statbuf.st_size         = 0;
                k_statbuf.st_blocks       = 0;
                k_statbuf.st_blksize      = 1024;
            }
            // stderr
            else if (fd == 2) {
                k_statbuf.st_dev          = 0x17;
                k_statbuf.st_ino          = 0xf;
                k_statbuf.st_mode         = 0x2190;
                k_statbuf.st_nlink        = 0x1;
                k_statbuf.st_uid          = 0x3e8;
                k_statbuf.st_gid          = 0x5;
                k_statbuf.st_rdev         = 0x880c;
                k_statbuf.st_size         = 0;
                k_statbuf.st_blocks       = 0;
                k_statbuf.st_blksize      = 1024;
            }
            else {
                emu->exit_reason = EMU_EXIT_REASON_FSTAT_BAD_FD;
                return;
            }

            // Write the statbuf into guest memory.
            const uint8_t write_ok = emu->mmu->write(emu->mmu, statbuf_guest_adr, (uint8_t*)&k_statbuf, sizeof(k_statbuf));
            if (write_ok != 0) {
                emu->exit_reason = EMU_EXIT_REASON_SEGFAULT_WRITE;
                break;
            }

            // Return success.
            emu->set_reg(emu, RISC_V_REG_A0, 0);
        }
        break;

    // exit. Emulator gracefully called the exit syscall. Graceful exit.
    case 93:
        emu->exit_reason = EMU_EXIT_REASON_GRACEFUL;
        break;

    // brk. Allocate/deallocate heap.
    case 214:
        {
            const uint64_t brk_val = emu->get_reg(emu, RISC_V_REG_A0);
            ginger_log(DEBUG, "brk address: 0x%lx\n", brk_val);

            if (brk_val == 0) {
                emu->set_reg(emu, RISC_V_REG_A0, emu->mmu->curr_alloc_adr);
                return;
            }

            // How much memory to allocate?
            const int64_t new_alloc_size = brk_val - emu->mmu->curr_alloc_adr;

            // TODO: Support freeing memory.
            if (new_alloc_size < 0) {
                ginger_log(ERROR, "brk. We do currently not support freeing memory!\n");
                exit(1);
            }

            if (emu->mmu->curr_alloc_adr + new_alloc_size > emu->mmu->memory_size) {
                ginger_log(ERROR, "brk. New allocation would run the emulator out of total memory!\n");
                exit(1);
            }

            uint8_t alloc_error = 0;
            const uint64_t heap_end = emu->mmu->allocate(emu->mmu, new_alloc_size, &alloc_error) + new_alloc_size;
            if (alloc_error != 0) {
                ginger_log(ERROR, "[%s] Failed to allocate memory on the heap!\n", __func__);
            }

            // Return the new brk address.
            emu->set_reg(emu, RISC_V_REG_A0, heap_end);
        }
        break;

    default:
        emu->exit_reason = EMU_EXIT_REASON_SYSCALL_NOT_SUPPORTED;
        break;
    }
}
