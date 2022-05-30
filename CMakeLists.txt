project(gingersnap)

cmake_minimum_required(VERSION 3.1)

add_executable(gingersnap
    src/corpus/corpus.c
    src/corpus/coverage.c
    src/debug_cli/debug_cli.c
    src/elf_loader/elf_loader.c
    src/elf_loader/program_header.c
    src/emu/emu_generic.c
    src/emu/emu_stats.c
    src/emu/mips64msb/mips64msb.c
    src/emu/riscv/riscv.c
    src/emu/riscv/syscall_riscv.c
    src/main/config.c
    src/main/main.c
    src/main/sig_handler.c
    src/mmu/adr_map.c
    src/mmu/mmu.c
    src/snap/snapshot_engine.c
    src/target/target.c
    src/utils/cli.c
    src/utils/dir.c
    src/utils/endianess.c
    src/utils/hash.c
    src/utils/hstring.c
    src/utils/logger.c
    src/utils/print_utils.c
    src/utils/token_str.c
    src/utils/vector.c
)

target_compile_options(gingersnap
    PRIVATE
    -Werror
    -Wall
    )

target_link_libraries(gingersnap
    pthread)
