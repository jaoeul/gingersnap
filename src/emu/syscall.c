#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "risc_v_emu.h"
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
handle_syscall(risc_v_emu_t* emu, const uint64_t num)
{
    switch(num) {

    // write. Only stdout and stderr are supported.
    case 64:
        {
            const uint64_t fd            = get_reg(emu, REG_A0);
            const uint64_t buf_guest_adr = get_reg(emu, REG_A1);
            const uint64_t len           = get_reg(emu, REG_A2);

            if (fd != 1 && fd != 2) {
                ginger_log(ERROR, "Write syscall is only supported for stdout and stderr file descriptors!\n");
                ginger_log(ERROR, "fd: %lu\n", fd);
                exit(1);
            }

            if (!GUEST_VERBOSE_PRINTS) {
                // Fake that all bytes were written.
                set_reg(emu, REG_A0, len);
                return;
            }

            uint8_t print_buf[len + 1];
            memset(print_buf, 0, len + 1);
            emu->mmu->read(emu->mmu, print_buf, buf_guest_adr, len);
            ginger_log(INFO, "Guest wrote: %s\n", print_buf);
            set_reg(emu, REG_A0, len);
        }
        break;

    // fstat. Write information about a file into emulator memory. We only support stdin, stdout
    // and stderr file descriptors for now.
    case 80:
        {
            const uint64_t fd                = get_reg(emu, REG_A0);
            const uint64_t statbuf_guest_adr = get_reg(emu, REG_A1);

            printf("fstat syscall\n");
            printf("fd: %lu\n", fd);
            printf("statbuf: 0x%lx\n", statbuf_guest_adr);

            struct kernel_stat k_statbuf; // From sys/stat.h.
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
                emu->exit_reason = EMU_EXIT_FSTAT_BAD_FD;
                return;
            }

            // Write the statbuf into guest memory.
            emu->mmu->write(emu->mmu, statbuf_guest_adr, (uint8_t*)&k_statbuf, sizeof(k_statbuf));

            // Return success.
            set_reg(emu, REG_A0, 0);
        }
        break;

    // brk. Allocate/deallocate heap.
    case 214:
        {
            const uint64_t brk_val = get_reg(emu, REG_A0);
            printf("brk address: 0x%lx\n", brk_val);

            if (brk_val == 0) {
                set_reg(emu, REG_A0, emu->mmu->curr_alloc_adr);
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

            const uint64_t heap_end = emu->mmu->allocate(emu->mmu, new_alloc_size) + new_alloc_size;

            // Return the new brk address.
            set_reg(emu, REG_A0, heap_end);
        }
        break;

    default:
        emu->exit_reason = EMU_EXIT_REASON_SYSCALL_NOT_SUPPORTED;
        break;
    }
}
