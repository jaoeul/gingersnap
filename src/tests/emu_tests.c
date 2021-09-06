#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../emu/riscv_emu.h"

typedef void (*test_fn)(void);

static void
test_emulator_reset_memory_success(void)
{
    const size_t test_memory_size = 1024 * 1024;

    // Create some emulators for testing
    rv_emu_t* source_emu      = emu_create(test_memory_size);
    rv_emu_t* destination_emu = emu_create(test_memory_size);

    // Fill the source emulators memory with 0x41
    memset(source_emu->mmu->memory, 0x41, test_memory_size);

    // Fill the destination emulators memory with 0x42
    memset(destination_emu->mmu->memory, 0x42, test_memory_size);

    // Reset the memory state of the destination emulator to that of the source emulator
    destination_emu->fork(destination_emu, source_emu);

    // Verify that the memory of the destination emulator is the same as that of the source emulator
    const int result = memcmp(destination_emu->mmu->memory, source_emu->mmu->memory, test_memory_size);
    assert(result == 0);

    // If the assert did not trigger we passed the test
    printf("Passed test [%s]!\n", __func__);

    // Clean up
    source_emu->destroy(source_emu);
    destination_emu->destroy(destination_emu);
}

static void
test_emulator_reset_memory_invalid_size(void)
{
    const size_t test_memory_size   = 1024 * 1024;
    const size_t erroneous_memory_size = 1025 * 1024;

    // Create some emulators for testing
    rv_emu_t* source_emu      = emu_create(erroneous_memory_size);
    rv_emu_t* destination_emu = emu_create(test_memory_size);

    // Fill the source emulators memory with 0x41
    memset(source_emu->mmu->memory, 0x41, erroneous_memory_size);

    // Fill the destination emulators memory with 0x42
    memset(destination_emu->mmu->memory, 0x42, test_memory_size);

    // Reset the memory state of the destination emulator to that of the source emulator
    const bool result = destination_emu->fork(destination_emu, source_emu);

    // Assert that the call to reset returned false
    assert(result == false);

    // If the assert did not trigger we passed the test
    printf("Passed test [%s]!\n", __func__);

    // Clean up
    source_emu->destroy(source_emu);
    destination_emu->destroy(destination_emu);
}

static void
test_emulator_reset_registers_success(void)
{
    const size_t test_memory_size = 1024 * 1024;

    // Create some emulators for testing
    rv_emu_t* source_emu      = emu_create(test_memory_size);
    rv_emu_t* destination_emu = emu_create(test_memory_size);

    // Assign dummy values to source emulators registers
    source_emu->registers.zero = 0;
    source_emu->registers.ra   = 1;
    source_emu->registers.sp   = 2;
    source_emu->registers.gp   = 3;
    source_emu->registers.tp   = 4;
    source_emu->registers.t0   = 5;
    source_emu->registers.t1   = 6;
    source_emu->registers.t2   = 7;
    source_emu->registers.fp   = 8;
    source_emu->registers.s1   = 9;
    source_emu->registers.a0   = 10;
    source_emu->registers.a1   = 11;
    source_emu->registers.a2   = 12;
    source_emu->registers.a3   = 13;
    source_emu->registers.a4   = 14;
    source_emu->registers.a5   = 15;
    source_emu->registers.a6   = 16;
    source_emu->registers.a7   = 17;
    source_emu->registers.s2   = 18;
    source_emu->registers.s3   = 19;
    source_emu->registers.s4   = 20;
    source_emu->registers.s5   = 21;
    source_emu->registers.s6   = 22;
    source_emu->registers.s7   = 23;
    source_emu->registers.s8   = 24;
    source_emu->registers.s9   = 25;
    source_emu->registers.s10  = 26;
    source_emu->registers.s11  = 27;
    source_emu->registers.t3   = 28;
    source_emu->registers.t4   = 29;
    source_emu->registers.t5   = 30;
    source_emu->registers.t6   = 31;
    source_emu->registers.pc   = 32;

    // Reset the memory state of the destination emulator to that of the source emulator
    destination_emu->fork(destination_emu, source_emu);

    // Verify that the memory of the destination emulator is the same as that of the source emulator
    const int result = memcmp(&destination_emu->registers, &source_emu->registers, sizeof(destination_emu->registers));
    assert(result == 0);

    // If the assert did not trigger we passed the test
    printf("Passed test [%s]!\n", __func__);

    // Clean up
    source_emu->destroy(source_emu);
    destination_emu->destroy(destination_emu);
}

static void
test_emulator_allocate_memory_success(void)
{
    // Arrange
    const size_t test_memory_size = 1024 * 1024;
    rv_emu_t* emu = emu_create(test_memory_size);
    const size_t base = emu->mmu->current_allocation;
    const size_t new_allocation = test_memory_size / 2;

    // Act
    emu->mmu->allocate(emu->mmu, new_allocation);

    // Assert that the variable tracking the size of allocated memory has been
    // updated correctly
    assert(emu->mmu->current_allocation - base == new_allocation);
    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu->destroy(emu);
}

static void
test_emulator_allocate_memory_failure_out_of_memory(void)
{
    // Arrange
    const size_t test_memory_size = 1024 * 1024;
    rv_emu_t* emu = emu_create(test_memory_size);
    const size_t base = emu->mmu->current_allocation;
    const size_t new_allocation = test_memory_size;

    // Act
    emu->mmu->allocate(emu->mmu, new_allocation);

    // Assert that the variable tracking the size of allocated memory has been updated correctly
    assert(emu->mmu->current_allocation == base);
    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu->destroy(emu);
}

static void
test_emulator_reallocate_memory_success(void)
{
    // Arrange
    const size_t test_memory_size = 1024 * 1024;
    rv_emu_t* emu = emu_create(test_memory_size);
    const size_t base = emu->mmu->current_allocation;
    const size_t first_allocation  = 1024;
    const size_t second_allocation = 2048;

    // Act
    emu->mmu->allocate(emu->mmu, first_allocation);
    emu->mmu->allocate(emu->mmu, second_allocation);

    // Assert that the variable tracking the size of allocated memory has been updated correctly
    assert((emu->mmu->current_allocation - base) == (first_allocation + second_allocation));
    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu->destroy(emu);
}

static void
test_emulator_allocate_memory_test_permissions_success(void)
{
    // Arrange
    const size_t test_memory_size = 1024 * 1024;
    rv_emu_t* emu = emu_create(test_memory_size);
    const size_t base = emu->mmu->current_allocation;
    const size_t allocation_size = 32;

    uint8_t* expected = calloc(allocation_size, sizeof(uint8_t));
    memset(expected, PERM_WRITE | PERM_RAW, allocation_size);

    // Act
    emu->mmu->allocate(emu->mmu, allocation_size);
    const int result = memcmp((unsigned char *)emu->mmu->permissions + base, expected, allocation_size);

    // Assert that the newly allocated memory has the permissions set for uninitialized memory
    assert(result == 0);
    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu->destroy(emu);
    free(expected);
}

// Tries to read unintialized memory. Should fail
static void
test_emulator_allocate_memory_test_permissions_failure(void)
{
    // Arrange
    const size_t test_memory_size = 1024 * 1024;
    rv_emu_t* emu = emu_create(test_memory_size);
    const size_t allocation_size = 32;
    uint8_t      buffer[32];

    // Act
    emu->mmu->allocate(emu->mmu, allocation_size);
    emu->mmu->read(emu->mmu, buffer, 0, sizeof(buffer));

    // Assert that the newly allocated memory has the permissions set for uninitialized memory
    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu->destroy(emu);
}

static void
test_emulator_write_success(void)
{
    // Arrange
    const size_t test_memory_size    = 1024 * 1024;
    rv_emu_t* emu                = emu_create(test_memory_size);
    const size_t allocation_size     = 32;
    const size_t buffer_size         = 16;
    const size_t base_address        = emu->mmu->current_allocation;

    emu->mmu->allocate(emu->mmu, allocation_size);

    uint8_t buffer[buffer_size];
    memset(buffer, 41, buffer_size);

    // Act
    // Write to guest memory from the buffer
    emu->mmu->write(emu->mmu, base_address, buffer, buffer_size);
    const int result_int = memcmp(emu->mmu->memory + base_address, buffer, buffer_size);

    // Assert
    assert(result_int == 0);

    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu->destroy(emu);
}

static void
test_emulator_write_read_success(void)
{
    // Arrange
    const size_t test_memory_size    = 1024 * 1024;
    rv_emu_t* emu                = emu_create(test_memory_size);
    const size_t allocation_size     = 32;
    const size_t buffer_size         = 16;
    const size_t base                = emu->mmu->current_allocation;

    emu->mmu->allocate(emu->mmu, allocation_size);

    uint8_t source_buffer[buffer_size];
    memset(&source_buffer, 0x41, buffer_size);

    // Act
    // Write to guest memory from the buffer
    emu->mmu->write(emu->mmu, base, source_buffer, buffer_size);

    uint8_t dest_buffer[buffer_size];
    memset(&dest_buffer, 0, buffer_size);

    emu->mmu->read(emu->mmu, dest_buffer, base, buffer_size);

    // Assert
    const int result_int = memcmp(&dest_buffer, &source_buffer, buffer_size);

    assert(result_int == 0);

    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu->destroy(emu);
}

// Tries to read 16 bytes 100 bytes into memory. Only 16 bytes have been written. Should fail
static void
test_emulator_write_read_failure(void)
{
    // Arrange
    const size_t test_memory_size    = 1024 * 1024;
    rv_emu_t* emu                = emu_create(test_memory_size);
    const size_t allocation_size     = 128;
    const size_t buffer_size         = 16;

    emu->mmu->allocate(emu->mmu, allocation_size);
    uint8_t source_buffer[buffer_size];
    memset(&source_buffer, 0x41, buffer_size);

    // Act
    // Write to guest memory from the buffer
    emu->mmu->write(emu->mmu, 0, (uint8_t*)source_buffer, buffer_size);

    uint8_t dest_buffer[buffer_size];
    memset(&dest_buffer, 0, buffer_size);

    emu->mmu->read(emu->mmu, (uint8_t*)dest_buffer, 100, buffer_size);

    // Assert
    const int result_int = memcmp(&dest_buffer, &source_buffer, buffer_size);
    assert(result_int != 0);
    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu->destroy(emu);
}

static void
test_emulator_write_failure_permission_denied(void)
{
    // Arrange
    const size_t test_memory_size    = 1024 * 1024;
    rv_emu_t* emu                = emu_create(test_memory_size);
    const size_t allocation_size     = 128;
    const size_t buffer_size         = 32;

    emu->mmu->allocate(emu->mmu, allocation_size);

    uint8_t buffer[buffer_size];
    memset(buffer, 41, buffer_size);

    // Act
    // Make allocated memory not writeable
    emu->mmu->set_permissions(emu->mmu, 0, PERM_READ, buffer_size);

    // Write to guest memory from the buffer
    emu->mmu->write(emu->mmu, 0, buffer, buffer_size);

    // Assert
    const int result_int = memcmp(emu->mmu->memory, buffer, buffer_size);
    assert(result_int != 0);

    printf("Passed test [%s]!\n", __func__);

    // Clean up
    emu->destroy(emu);
}

#define NB_TEST_CASES 12

// TODO: Better RNG
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
        test_emulator_reset_memory_success,
        test_emulator_reset_memory_invalid_size,
        test_emulator_reset_registers_success,
        test_emulator_allocate_memory_success,
        test_emulator_allocate_memory_failure_out_of_memory,
        test_emulator_reallocate_memory_success,
        test_emulator_allocate_memory_test_permissions_success,
        test_emulator_write_success,
        test_emulator_write_read_success,
        test_emulator_write_failure_permission_denied,
        test_emulator_write_read_failure,
        test_emulator_allocate_memory_test_permissions_failure,
    };

    // Execute tests in random order
    shuffle_tests(test_cases);
    for (int i = 0; i < NB_TEST_CASES; i++) {
        printf("TEST: [%d]\t", i);
        ((void (*)())test_cases[i])();
    }

    return 0;
}
