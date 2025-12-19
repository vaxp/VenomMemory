/*
 * VenomMemory - High-Performance Shared Memory IPC Library
 * 
 * venom_sync.h - Synchronization and signaling primitives
 */

#ifndef VENOM_SYNC_H
#define VENOM_SYNC_H

#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Timeout values */
#define VENOM_WAIT_FOREVER  -1
#define VENOM_NO_WAIT        0

/* ============================================
 * Futex-based signaling (for shared memory)
 * ============================================ */

/**
 * Wait until the value at ptr changes from expected_val
 * Uses Linux futex for efficient cross-process waiting
 * 
 * @param ptr           Pointer to atomic value in shared memory
 * @param expected_val  Value to wait on (wait while *ptr == expected_val)
 * @param timeout_ms    Timeout in milliseconds (-1 = infinite)
 * @return              0 on value change, -1 on error, -2 on timeout
 */
int venom_futex_wait(_Atomic uint32_t *ptr, uint32_t expected_val, int timeout_ms);

/**
 * Wake up one waiter on the futex
 * 
 * @param ptr   Pointer to atomic value in shared memory
 * @return      Number of waiters woken, or -1 on error
 */
int venom_futex_wake(_Atomic uint32_t *ptr);

/**
 * Wake up all waiters on the futex
 * 
 * @param ptr   Pointer to atomic value in shared memory
 * @return      Number of waiters woken, or -1 on error
 */
int venom_futex_wake_all(_Atomic uint32_t *ptr);

/* ============================================
 * Atomic operations for lock-free programming
 * ============================================ */

/**
 * Atomic compare and swap for uint32_t
 * 
 * @param ptr       Pointer to atomic value
 * @param expected  Expected current value
 * @param desired   Desired new value
 * @return          1 if swap succeeded, 0 if failed
 */
static inline int venom_atomic_cas_u32(_Atomic uint32_t *ptr, 
                                        uint32_t expected, 
                                        uint32_t desired) {
    return atomic_compare_exchange_strong(ptr, &expected, desired);
}

/**
 * Atomic load for uint32_t
 */
static inline uint32_t venom_atomic_load_u32(_Atomic uint32_t *ptr) {
    return atomic_load(ptr);
}

/**
 * Atomic store for uint32_t
 */
static inline void venom_atomic_store_u32(_Atomic uint32_t *ptr, uint32_t val) {
    atomic_store(ptr, val);
}

/**
 * Atomic load for size_t
 */
static inline size_t venom_atomic_load_size(_Atomic size_t *ptr) {
    return atomic_load(ptr);
}

/**
 * Atomic store for size_t
 */
static inline void venom_atomic_store_size(_Atomic size_t *ptr, size_t val) {
    atomic_store(ptr, val);
}

/**
 * Memory fence - ensure all previous operations are visible
 */
static inline void venom_memory_barrier(void) {
    atomic_thread_fence(memory_order_seq_cst);
}

#ifdef __cplusplus
}
#endif

#endif /* VENOM_SYNC_H */
