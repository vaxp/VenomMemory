/*
 * VenomMemory Test - Shared Memory
 * 
 * Tests for basic shared memory operations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "venom_memory.h"

#define TEST(name) printf("Testing: %s... ", name)
#define PASS() printf("PASSED\n")
#define FAIL(msg) do { printf("FAILED: %s\n", msg); failures++; } while(0)

static int failures = 0;

void test_shm_create_close(void) {
    TEST("shm_create and shm_close");
    
    VenomShm *shm = venom_shm_create("test_basic", 4096);
    if (!shm) {
        FAIL("Failed to create shared memory");
        return;
    }
    
    if (venom_shm_size(shm) != 4096) {
        FAIL("Size mismatch");
        venom_shm_close(shm);
        venom_shm_unlink("test_basic");
        return;
    }
    
    venom_shm_close(shm);
    venom_shm_unlink("test_basic");
    PASS();
}

void test_shm_open(void) {
    TEST("shm_open existing");
    
    /* Create */
    VenomShm *shm1 = venom_shm_create("test_open", 8192);
    if (!shm1) {
        FAIL("Failed to create shared memory");
        return;
    }
    
    /* Write data */
    const char *test_data = "Hello, VenomMemory!";
    memcpy(venom_shm_addr(shm1), test_data, strlen(test_data) + 1);
    
    /* Open from another handle */
    VenomShm *shm2 = venom_shm_open("test_open");
    if (!shm2) {
        FAIL("Failed to open shared memory");
        venom_shm_close(shm1);
        venom_shm_unlink("test_open");
        return;
    }
    
    /* Verify data */
    if (strcmp(venom_shm_addr(shm2), test_data) != 0) {
        FAIL("Data mismatch");
        venom_shm_close(shm1);
        venom_shm_close(shm2);
        venom_shm_unlink("test_open");
        return;
    }
    
    venom_shm_close(shm1);
    venom_shm_close(shm2);
    venom_shm_unlink("test_open");
    PASS();
}

void test_shm_unlink(void) {
    TEST("shm_unlink");
    
    VenomShm *shm = venom_shm_create("test_unlink", 4096);
    if (!shm) {
        FAIL("Failed to create shared memory");
        return;
    }
    venom_shm_close(shm);
    
    int result = venom_shm_unlink("test_unlink");
    if (result != VENOM_OK) {
        FAIL("Failed to unlink");
        return;
    }
    
    /* Try to open - should fail */
    shm = venom_shm_open("test_unlink");
    if (shm) {
        FAIL("Should not be able to open after unlink");
        venom_shm_close(shm);
        return;
    }
    
    PASS();
}

void test_shm_large(void) {
    TEST("large shared memory (1MB)");
    
    size_t size = 1024 * 1024;  /* 1MB */
    VenomShm *shm = venom_shm_create("test_large", size);
    if (!shm) {
        FAIL("Failed to create large shared memory");
        return;
    }
    
    /* Write pattern */
    uint8_t *ptr = (uint8_t*)venom_shm_addr(shm);
    for (size_t i = 0; i < size; i++) {
        ptr[i] = (uint8_t)(i & 0xFF);
    }
    
    /* Verify pattern */
    int ok = 1;
    for (size_t i = 0; i < size && ok; i++) {
        if (ptr[i] != (uint8_t)(i & 0xFF)) {
            ok = 0;
        }
    }
    
    venom_shm_close(shm);
    venom_shm_unlink("test_large");
    
    if (!ok) {
        FAIL("Pattern verification failed");
        return;
    }
    
    PASS();
}

int main(void) {
    printf("=== VenomMemory Shared Memory Tests ===\n\n");
    
    test_shm_create_close();
    test_shm_open();
    test_shm_unlink();
    test_shm_large();
    
    printf("\n=== Results: %d failures ===\n", failures);
    return failures > 0 ? 1 : 0;
}
