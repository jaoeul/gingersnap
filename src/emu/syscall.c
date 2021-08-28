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

    // fstat. Write information about a file into emulator memory. We only support stdin, stdout
    // and stderr file descriptors for now.
    case 80:
        {
            const uint64_t fd                = get_register(emu, REG_A0);
            const uint64_t statbuf_guest_adr = get_register(emu, REG_A1);

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
            set_register(emu, REG_A0, 0);
        }
        break;

    // brk. Extend the processe data segment.
    case 214:
        ginger_log(ERROR, "brk not yet implemented!\n");
        exit(1);
        break;

    default:
        emu->exit_reason = EMU_EXIT_REASON_SYSCALL_NOT_SUPPORTED;
        break;
    }
}
