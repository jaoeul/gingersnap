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

void
handle_syscall(risc_v_emu_t* emu, const uint64_t num)
{
    switch(num) {

    // write.
    case 64:
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

            struct stat statbuf; // From sys/stat.h.
            memset(&statbuf, 0, sizeof(statbuf));

            // Following magic values were retreived from running fstat on the host OS.
            // stdin
            if (fd == 0) {
                statbuf.st_dev          = 0x17;
                statbuf.st_ino          = 0x6;
                statbuf.st_mode         = 0x2190;
                statbuf.st_nlink        = 0x1;
                statbuf.st_uid          = 0x3e8;
                statbuf.st_gid          = 0x5;
                statbuf.st_rdev         = 0x8803;
                statbuf.st_size         = 0;
                statbuf.st_blksize      = 1024;
                statbuf.st_blocks       = 0;
                statbuf.st_atim.tv_sec  = 0;
                statbuf.st_atim.tv_nsec = 0;
                statbuf.st_mtim.tv_sec  = 0;
                statbuf.st_mtim.tv_nsec = 0;
                statbuf.st_ctim.tv_sec  = 0;
                statbuf.st_ctim.tv_nsec = 0;
            }
            // stdout
            else if (fd == 1) {
                statbuf.st_dev          = 0x17;
                statbuf.st_ino          = 0xe;
                statbuf.st_mode         = 0x2190;
                statbuf.st_nlink        = 0x1;
                statbuf.st_uid          = 0x3e8;
                statbuf.st_gid          = 0x5;
                statbuf.st_rdev         = 0x880b;
                statbuf.st_size         = 0;
                statbuf.st_blksize      = 1024;
                statbuf.st_blocks       = 0;
                statbuf.st_atim.tv_sec  = 0;
                statbuf.st_atim.tv_nsec = 0;
                statbuf.st_mtim.tv_sec  = 0;
                statbuf.st_mtim.tv_nsec = 0;
                statbuf.st_ctim.tv_sec  = 0;
                statbuf.st_ctim.tv_nsec = 0;
            }
            // stderr
            else if (fd == 2) {
                statbuf.st_dev          = 0x17;
                statbuf.st_ino          = 0xf;
                statbuf.st_mode         = 0x2190;
                statbuf.st_nlink        = 0x1;
                statbuf.st_uid          = 0x3e8;
                statbuf.st_gid          = 0x5;
                statbuf.st_rdev         = 0x880c;
                statbuf.st_size         = 0;
                statbuf.st_blksize      = 1024;
                statbuf.st_blocks       = 0;
                statbuf.st_atim.tv_sec  = 0;
                statbuf.st_atim.tv_nsec = 0;
                statbuf.st_mtim.tv_sec  = 0;
                statbuf.st_mtim.tv_nsec = 0;
                statbuf.st_ctim.tv_sec  = 0;
                statbuf.st_ctim.tv_nsec = 0;
            }
            else {
                emu->exit_reason = EMU_EXIT_FSTAT_BAD_FD;
                return;
            }

            // Write the statbuf into guest memory.
            emu->mmu->write(emu->mmu, statbuf_guest_adr, (uint8_t*)&statbuf, sizeof(statbuf));

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
