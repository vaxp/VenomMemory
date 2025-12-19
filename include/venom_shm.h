/*
 * VenomMemory - High-Performance Shared Memory IPC Library
 * Copyright (c) 2024
 * 
 * venom_shm.h - Low-level shared memory operations
 */

#ifndef VENOM_SHM_H
#define VENOM_SHM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Shared memory handle */
typedef struct VenomShm {
    int fd;           /* File descriptor */
    void *addr;       /* Mapped address */
    size_t size;      /* Size of mapping */
    char name[256];   /* Name of shared memory */
    int is_owner;     /* 1 if creator, 0 if opener */
} VenomShm;

/* Error codes */
#define VENOM_OK            0
#define VENOM_ERR_CREATE   -1
#define VENOM_ERR_OPEN     -2
#define VENOM_ERR_MMAP     -3
#define VENOM_ERR_TRUNCATE -4
#define VENOM_ERR_INVALID  -5
#define VENOM_ERR_EXISTS   -6
#define VENOM_ERR_NOTFOUND -7

/**
 * Create a new shared memory region
 * 
 * @param name   Name of the shared memory (will be prefixed with /venom_)
 * @param size   Size in bytes
 * @return       Pointer to VenomShm on success, NULL on failure
 */
VenomShm* venom_shm_create(const char *name, size_t size);

/**
 * Open an existing shared memory region
 * 
 * @param name   Name of the shared memory
 * @return       Pointer to VenomShm on success, NULL on failure
 */
VenomShm* venom_shm_open(const char *name);

/**
 * Close and unmap a shared memory region
 * Does not remove the shared memory, just unmaps it
 * 
 * @param shm    Pointer to VenomShm
 */
void venom_shm_close(VenomShm *shm);

/**
 * Remove a shared memory region from the system
 * Should only be called by the creator (daemon)
 * 
 * @param name   Name of the shared memory
 * @return       VENOM_OK on success, error code on failure
 */
int venom_shm_unlink(const char *name);

/**
 * Get the mapped address of the shared memory
 * 
 * @param shm    Pointer to VenomShm
 * @return       Pointer to mapped memory
 */
static inline void* venom_shm_addr(VenomShm *shm) {
    return shm ? shm->addr : NULL;
}

/**
 * Get the size of the shared memory
 * 
 * @param shm    Pointer to VenomShm
 * @return       Size in bytes
 */
static inline size_t venom_shm_size(VenomShm *shm) {
    return shm ? shm->size : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* VENOM_SHM_H */
