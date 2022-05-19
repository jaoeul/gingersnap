project(gingersnap)

cmake_minimum_required(VERSION 3.1)

add_executable(gingersnap
    src/main/main.c
    src/main/sig_handler.c
    src/main/config.c
    src/debug_cli/debug_cli.c
    src/target/target.c
    src/mmu/mmu.c
    src/corpus/corpus.c
    src/corpus/coverage.c
    src/utils/dir.c
    src/utils/elf_loader.c
    src/utils/hstring.c
    src/utils/logger.c
    src/utils/print_utils.c
    src/utils/endianess_converter.c
    src/utils/hash.c
    src/utils/token_str.c
    src/utils/cli.c
    src/utils/vector.c
    src/emu/emu_generic.c
    src/emu/riscv/riscv.c
    src/emu/riscv/syscall_riscv.c
    src/emu/emu_stats.c
    src/fuzzer/fuzzer.c
)

target_link_libraries(gingersnap
    pthread)
