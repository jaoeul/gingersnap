#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../snap_engine/snapshot_engine.h"
#include "../emu/riscv_emu.h"
#include "../debug_cli/debug_cli.h"
#include "../utils/endianess.h"
#include "../utils/logger.h"
#include "../utils/print_utils.h"

typedef void (*test_fn)(void);

static rv_emu_t*
test_prepare_emu(uint64_t mem_size)
{
    rv_emu_t* emu         = emu_create(mem_size);
    const int target_argc = 1;

    // Array of arguments to the target executable.
    heap_str_t target_argv[target_argc];
    memset(target_argv, 0, sizeof(target_argv));

    // First arg.
    heap_str_t arg0;
    heap_str_set(&arg0, "./data/targets/bin/target4");
    target_argv[0] = arg0;

    // Prepare the target executable.
    target_t* target = target_create(target_argc, target_argv);

    if (mem_size < target->elf->length) {
        printf("Error, emulator mem to small for elf!\n");
        abort();
    }

    // Load the elf and build the stack.
    emu->setup(emu, target);

    return emu;
}

static void
test_emu_reset_mem_success(void)
{
    printf("Starting test [%s]!\n", __func__);
    const size_t mem_size  = (1024 * 1024) * 256;
    const size_t ut_buf_sz = 10;

    // Create some emulators for testing.
    rv_emu_t* emu_a = test_prepare_emu(mem_size);
    rv_emu_t* emu_b = test_prepare_emu(mem_size);

    // Write a bunch of 'A's into the first emulators memory.
    uint8_t alloc_error = 0;
    const uint64_t write_adr_a = emu_a->mmu->allocate(emu_a->mmu, ut_buf_sz, &alloc_error);
    if (alloc_error != 0) {
        ginger_log(ERROR, "[%s] Could not allocate memory for emulator!\n", __func__);
    }
    uint8_t buf_a[ut_buf_sz];
    memset(buf_a, 0x41, ut_buf_sz);
    emu_a->mmu->write(emu_a->mmu, write_adr_a, buf_a, ut_buf_sz);

    // Write a bunch of 'B's into the first emulators memory.
    const uint64_t write_adr_b = emu_a->mmu->allocate(emu_b->mmu, ut_buf_sz, &alloc_error);
    if (alloc_error != 0) {
        ginger_log(ERROR, "[%s] Could not allocate memory for emulator!\n", __func__);
    }
    uint8_t buf_b[ut_buf_sz];
    memset(buf_b, 0x42, ut_buf_sz);
    emu_b->mmu->write(emu_b->mmu, write_adr_b, buf_b, ut_buf_sz);

    // Reset the memory state of the destination emulator to that of the source emulator
    emu_a->reset(emu_a, emu_b);

    // Verify that the memory of the destination emulator is the same as that of the source emulator
    const int result = memcmp(emu_a->mmu->memory + write_adr_a, emu_b->mmu->memory + write_adr_b, ut_buf_sz);
    assert(result == 0);

    // If the assert did not trigger we passed the test
    ginger_log(INFO, "Passed test [%s]!\n", __func__);

    // Clean up
    emu_a->destroy(emu_a);
    emu_b->destroy(emu_b);
}

static void
test_emu_reset_registers_success(void)
{
    printf("Starting test [%s]!\n", __func__);
    const size_t test_memory_size = (1024 * 1024) * 256;

    // Create some emulators for testing.
    rv_emu_t* emu_a = test_prepare_emu(test_memory_size);
    rv_emu_t* emu_b = test_prepare_emu(test_memory_size);

    // Assign dummy values to source emulators registers.
    for (uint64_t i = 0; i < 33; i++) {
        emu_b->registers[i] = i;
    }

    // Reset the memory state of the destination emulator to that of the source emulator.
    emu_b->reset(emu_b, emu_a);

    // Verify that the memory of the destination emulator is the same as that of the source emulator.
    const int result = memcmp(&emu_b->registers, &emu_a->registers, sizeof(emu_b->registers));
    assert(result == 0);

    // If the assert did not trigger we passed the test
    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu_a->destroy(emu_a);
    emu_b->destroy(emu_b);
}

static void
test_emu_alloc_mem_success(void)
{
    printf("Starting test [%s]!\n", __func__);
    const size_t test_memory_size = (1024 * 1024) * 256;
    rv_emu_t*    emu              = test_prepare_emu(test_memory_size);
    const size_t base             = emu->mmu->curr_alloc_adr;
    const size_t alloc_sz         = (1024 * 1024) * 64;

    uint8_t alloc_error = 0;
    emu->mmu->allocate(emu->mmu, alloc_sz, &alloc_error);

    assert(alloc_error == ALLOC_NO_ERROR);

    // Assert that the variable tracking the size of allocated memory has been updated correctly.
    assert(emu->mmu->curr_alloc_adr - base == alloc_sz);

    // Assert that the allocation got the correct permissions.
    uint8_t alloc_perm = PERM_RAW | PERM_WRITE;
    for (uint64_t i = 0; i < alloc_sz; i++) {

        const int result = memcmp(emu->mmu->permissions + base + i, &alloc_perm, 1);
        if (result != 0) {
            ginger_log(ERROR, "Address: 0x%lx Perm: ", base + alloc_sz);
            print_permissions(emu->mmu->permissions[base + i]);
            printf("\n");
        }
        assert(result == 0);
    }

    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu->destroy(emu);
}

static void
test_emu_alloc_mem_would_overrun_failure(void)
{
    printf("Starting test [%s]!\n", __func__);
    const size_t test_memory_size = (1024 * 1024) * 256;
    rv_emu_t*    emu              = test_prepare_emu(test_memory_size);
    const size_t alloc_sz         = test_memory_size * 2;

    uint8_t alloc_error = 0;
    emu->mmu->allocate(emu->mmu, alloc_sz, &alloc_error);

    // Assert that the variable tracking the size of allocated memory has been updated correctly
    assert(alloc_error == ALLOC_ERROR_WOULD_OVERRUN);

    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu->destroy(emu);
}

static void
test_emu_alloc_mem_full_failure(void)
{
    printf("Starting test [%s]!\n", __func__);
    const size_t test_memory_size = (1024 * 1024) * 256;
    rv_emu_t*    emu              = test_prepare_emu(test_memory_size);
    const size_t alloc_sz         = 1;

    // Fake that memory is full.
    emu->mmu->curr_alloc_adr = emu->mmu->memory_size;

    uint8_t alloc_error = 0;
    emu->mmu->allocate(emu->mmu, alloc_sz, &alloc_error);

    // Assert that the variable tracking the size of allocated memory has been updated correctly
    assert(alloc_error == ALLOC_ERROR_MEM_FULL);
    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu->destroy(emu);
}

static void
test_emu_two_allocs_success(void)
{
    printf("Starting test [%s]!\n", __func__);
    const size_t test_memory_size  = (1024 * 1024) * 256;
    rv_emu_t*    emu               = test_prepare_emu(test_memory_size);
    const size_t base              = emu->mmu->curr_alloc_adr;
    const size_t first_allocation  = 1024;
    const size_t second_allocation = 2048;

    uint8_t alloc_error = 0;
    emu->mmu->allocate(emu->mmu, first_allocation, &alloc_error);
    emu->mmu->allocate(emu->mmu, second_allocation, &alloc_error);

    // Assert that allocations worked.
    assert(alloc_error == ALLOC_NO_ERROR);

    // Assert that the variable tracking the size of allocated memory has been updated correctly
    assert((emu->mmu->curr_alloc_adr - base) == (first_allocation + second_allocation));
    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu->destroy(emu);
}

// Tries to read unintialized memory.
static void
test_emu_read_illegal_address_failure(void)
{
    printf("Starting test [%s]!\n", __func__);
    const size_t test_memory_size = (1024 * 1024) * 256;
    rv_emu_t*    emu              = test_prepare_emu(test_memory_size);
    const size_t alloc_sz         = 32;
    uint8_t      buffer[32];

    uint8_t alloc_error = 0;
    emu->mmu->allocate(emu->mmu, alloc_sz, &alloc_error);
    const uint8_t result = emu->mmu->read(emu->mmu, buffer, 0, sizeof(buffer));

    assert(result == READ_ERROR_NO_PERM);
    assert(alloc_error == ALLOC_NO_ERROR);

    // Assert that the newly allocated memory has the permissions set for uninitialized memory
    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu->destroy(emu);
}

// Try to write outside of allocated memory.
static void
test_emu_write_illegal_address_failure(void)
{
    printf("Starting test [%s]!\n", __func__);
    const size_t test_memory_size = (1024 * 1024) * 256;
    rv_emu_t*    emu              = test_prepare_emu(test_memory_size);
    const size_t alloc_sz         = 32;
    uint8_t      buffer[32]       = {0x41};

    uint8_t alloc_error = 0;
    emu->mmu->allocate(emu->mmu, alloc_sz, &alloc_error);
    const uint8_t result = emu->mmu->write(emu->mmu, alloc_sz, buffer, sizeof(buffer));

    assert(result == WRITE_ERROR_NO_PERM);
    assert(alloc_error == ALLOC_NO_ERROR);

    // Assert that the newly allocated memory has the permissions set for uninitialized memory
    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu->destroy(emu);
}

static void
test_emu_write_success(void)
{
    printf("Starting test [%s]!\n", __func__);
    const size_t test_memory_size = (1024 * 1024) * 256;
    rv_emu_t*    emu              = test_prepare_emu(test_memory_size);
    const size_t alloc_sz         = 32;
    const size_t buf_sz           = 16;
    const size_t base_address     = emu->mmu->curr_alloc_adr;

    uint8_t alloc_error = 0;
    emu->mmu->allocate(emu->mmu, alloc_sz, &alloc_error);

    uint8_t buffer[buf_sz];
    memset(buffer, 41, buf_sz);

    // Write to guest memory from the buffer
    emu->mmu->write(emu->mmu, base_address, buffer, buf_sz);
    const int result = memcmp(emu->mmu->memory + base_address, buffer, buf_sz);

    assert(result == 0);
    assert(alloc_error == ALLOC_NO_ERROR);

    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu->destroy(emu);
}

static void
test_emu_write_read_success(void)
{
    const size_t test_memory_size    = (1024 * 1024) * 256;
    rv_emu_t*    emu                 = test_prepare_emu(test_memory_size);
    const size_t allocation_size     = 32;
    const size_t buffer_size         = 16;
    const size_t base                = emu->mmu->curr_alloc_adr;
    uint8_t      alloc_error         = 0;

    emu->mmu->allocate(emu->mmu, allocation_size, &alloc_error);

    uint8_t source_buffer[buffer_size];
    memset(&source_buffer, 0x41, buffer_size);

    // Write to guest memory from the buffer
    emu->mmu->write(emu->mmu, base, source_buffer, buffer_size);

    uint8_t dest_buffer[buffer_size];
    memset(&dest_buffer, 0, buffer_size);

    emu->mmu->read(emu->mmu, dest_buffer, base, buffer_size);

    const int result = memcmp(&dest_buffer, &source_buffer, buffer_size);

    assert(result == 0);
    assert(alloc_error == ALLOC_NO_ERROR);

    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu->destroy(emu);
}

// Tries to read 16 bytes 100 bytes into memory. Only 16 bytes have been written. Should fail.
static void
test_emu_write_read_failure(void)
{
    const size_t   test_memory_size = (1024 * 1024) * 256;
    rv_emu_t*      emu              = test_prepare_emu(test_memory_size);
    const size_t   allocation_size  = 128;
    const size_t   buffer_size      = 16;
    uint8_t        alloc_error      = 0;
    const uint64_t base             = emu->mmu->allocate(emu->mmu, allocation_size, &alloc_error);

    uint8_t source_buffer[buffer_size];
    memset(&source_buffer, 0x41, buffer_size);

    // Write to guest memory from the buffer
    const uint8_t write_res = emu->mmu->write(emu->mmu, base, (uint8_t*)source_buffer, buffer_size);

    uint8_t dest_buffer[buffer_size];
    memset(&dest_buffer, 0, buffer_size);

    const uint8_t read_res = emu->mmu->read(emu->mmu, (uint8_t*)dest_buffer, base + 100, buffer_size);

    const int result = memcmp(&dest_buffer, &source_buffer, buffer_size);

    assert(result      != 0);
    assert(alloc_error == ALLOC_NO_ERROR);
    assert(write_res   == WRITE_NO_ERROR);
    assert(read_res    == READ_ERROR_NO_PERM);

    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu->destroy(emu);
}

static void
test_emu_write_failure_perm_denied_failure(void)
{
    const size_t test_memory_size    = (1024 * 1024) * 256;
    rv_emu_t*    emu                 = test_prepare_emu(test_memory_size);
    const size_t alloc_sz            = 128;
    const size_t buffer_size         = 32;
    uint8_t      alloc_error         = 0;

    emu->mmu->allocate(emu->mmu, alloc_sz, &alloc_error);

    uint8_t buffer[buffer_size];
    memset(buffer, 41, buffer_size);

    // Make allocated memory not writeable
    emu->mmu->set_permissions(emu->mmu, 0, PERM_READ, buffer_size);

    // Try to write to non writeable memory.
    const uint8_t write_res = emu->mmu->write(emu->mmu, 0, buffer, buffer_size);

    const int result_int = memcmp(emu->mmu->memory, buffer, buffer_size);

    assert(write_res   == WRITE_ERROR_NO_PERM);
    assert(result_int  != 0);
    assert(alloc_error == ALLOC_NO_ERROR);

    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu->destroy(emu);
}

static void
test_fuzzer_target4_segault(void)
{
    // Create target.
    const int target_argc = 1;
    heap_str_t target_argv[target_argc];
    memset(target_argv, 0, sizeof(target_argv));
    heap_str_t arg0;
    heap_str_set(&arg0, "./data/targets/bin/target4");
    target_argv[0]   = arg0;
    target_t* target = target_create(target_argc, target_argv);

    // Create snapshot required by fuzzer.
    rv_emu_t* snapshot = emu_create(1024 * 1024 * 256);
    snapshot->setup(snapshot, target);

    // Prepare the target executable.
    corpus_t* corpus = corpus_create("./data/corpus/test_corpus");

    // The address of the value which is compared to `1337` in emulator memory,
    // when running `target4`.
    const uint64_t fuzz_buf_adr  = 0x15878;
    const uint64_t fuzz_buf_size = 4;

    fuzzer_t* fuzzer = fuzzer_create(corpus, fuzz_buf_adr, fuzz_buf_size, target, snapshot);

    // 0x539 = 1337, Insert as LSB byte array into fuzzer emulator memory.
    uint8_t input_data[2] = { 0x39, 0x5 };

    // Inject the above buffer.
    fuzzer->inject(fuzzer, input_data, 2);

    // Run the program loaded into the fuzzer. If the value got injected properly, the target
    // executable should segfault.
    fuzzer->emu->run(fuzzer->emu, fuzzer->stats);

    assert(fuzzer->emu->exit_reason == EMU_EXIT_REASON_SEGFAULT_READ);
}

static void
test_fuzzer_target4_gracefull(void)
{
    // Create target.
    const int target_argc = 1;
    heap_str_t target_argv[target_argc];
    memset(target_argv, 0, sizeof(target_argv));
    heap_str_t arg0;
    heap_str_set(&arg0, "./data/targets/bin/target4");
    target_argv[0]   = arg0;
    target_t* target = target_create(target_argc, target_argv);

    // Prepare the target executable.
    corpus_t* corpus = corpus_create("./data/corpus/test_corpus");

    fuzzer_t* fuzzer = fuzzer_create(corpus, 0, 0, target, NULL);

    // Run the program loaded into the fuzzer. If the value got injected properly, the target
    // executable should segfault.
    fuzzer->emu->run(fuzzer->emu, fuzzer->stats);

    assert(fuzzer->emu->exit_reason == EMU_EXIT_REASON_GRACEFUL);
}

// Could fail if we are unlucky with the RNG.
static void
test_fuzzer_mutate(void)
{
    // Create target.
    const int target_argc = 1;
    heap_str_t target_argv[target_argc];
    memset(target_argv, 0, sizeof(target_argv));
    heap_str_t arg0;
    heap_str_set(&arg0, "./data/targets/bin/target4");
    target_argv[0]   = arg0;
    target_t* target = target_create(target_argc, target_argv);

    // Prepare the target executable.
    corpus_t* corpus = corpus_create("./data/corpus/test_corpus");

    fuzzer_t* fuzzer = fuzzer_create(corpus, 0, 1024, target, NULL);

    char a[1024] = {0};
    char b[1024] = {0};
    int res = memcmp(a, b, 1024);
    assert(res == 0);

    // Hopefully this call mutates atleast one byte to a new value...
    fuzzer->mutate(a, 1024);

    res = memcmp(a, b, 1024);
    assert(res != 0);
}

#define NB_TEST_CASES 15

// TODO: Better RNG
__attribute__((used))
static void
shuffle_tests(test_fn test_cases[])
{
    srand(time(NULL));
    for (int i = NB_TEST_CASES - 1; i > 0; i--)
    {
        int j = rand() % (i+1);
        test_fn tmp = test_cases[i];

        test_cases[i] = test_cases[j];
        test_cases[j] = tmp;
        (void)j;
    }
}

int
main(void)
{
    // Create an array of function pointers. Each pointer points to a test case.
    void (*test_cases[NB_TEST_CASES])() = {
        test_emu_reset_mem_success,
        test_emu_reset_registers_success,
        test_emu_alloc_mem_success,
        test_emu_alloc_mem_would_overrun_failure,
        test_emu_alloc_mem_full_failure,
        test_emu_two_allocs_success,
        test_emu_read_illegal_address_failure,
        test_emu_write_illegal_address_failure,
        test_emu_write_success,
        test_emu_write_read_success,
        test_emu_write_read_failure,
        test_emu_write_failure_perm_denied_failure,
        test_fuzzer_target4_segault,
        test_fuzzer_target4_gracefull,
        test_fuzzer_mutate,
    };

    // Execute tests in random order
    shuffle_tests(test_cases);
    for (int i = 0; i < NB_TEST_CASES; i++) {
        ((void (*)())test_cases[i])();
        printf("PASSED TEST: [%d]\n", i + 1);
    }

    return 0;
}
