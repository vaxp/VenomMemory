/*
 * VenomMemory - High-Performance Shared Memory IPC Library
 * 
 * venom_sync.c - Synchronization implementation using futex
 */

#define _GNU_SOURCE
#include "venom_sync.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <linux/futex.h>
#include <sys/syscall.h>

/* Futex syscall wrapper */
static int futex(uint32_t *uaddr, int futex_op, uint32_t val,
                 const struct timespec *timeout, uint32_t *uaddr2, uint32_t val3) {
    return syscall(SYS_futex, uaddr, futex_op, val, timeout, uaddr2, val3);
}

int venom_futex_wait(_Atomic uint32_t *ptr, uint32_t expected_val, int timeout_ms) {
    struct timespec ts;
    struct timespec *ts_ptr = NULL;
    
    if (timeout_ms >= 0) {
        ts.tv_sec = timeout_ms / 1000;
        ts.tv_nsec = (timeout_ms % 1000) * 1000000L;
        ts_ptr = &ts;
    }
    
    /* Cast away _Atomic for futex syscall - safe because futex handles atomicity */
    int ret = futex((uint32_t*)ptr, FUTEX_WAIT, expected_val, ts_ptr, NULL, 0);
    
    if (ret == -1) {
        if (errno == EAGAIN) {
            /* Value already changed */
            return 0;
        }
        if (errno == ETIMEDOUT) {
            return -2;  /* Timeout */
        }
        return -1;  /* Error */
    }
    
    return 0;  /* Woken up */
}

int venom_futex_wake(_Atomic uint32_t *ptr) {
    return futex((uint32_t*)ptr, FUTEX_WAKE, 1, NULL, NULL, 0);
}

int venom_futex_wake_all(_Atomic uint32_t *ptr) {
    return futex((uint32_t*)ptr, FUTEX_WAKE, INT32_MAX, NULL, NULL, 0);
}
