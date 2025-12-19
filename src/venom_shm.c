/*
 * VenomMemory - High-Performance Shared Memory IPC Library
 * 
 * venom_shm.c - Low-level shared memory implementation
 */

#define _GNU_SOURCE
#include "venom_shm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>

#define VENOM_SHM_PREFIX "/venom_"

/* Build full shared memory name */
static void build_shm_name(char *dest, size_t dest_size, const char *name) {
    snprintf(dest, dest_size, "%s%s", VENOM_SHM_PREFIX, name);
}

VenomShm* venom_shm_create(const char *name, size_t size) {
    if (!name || size == 0) {
        return NULL;
    }

    VenomShm *shm = (VenomShm*)calloc(1, sizeof(VenomShm));
    if (!shm) {
        return NULL;
    }

    build_shm_name(shm->name, sizeof(shm->name), name);
    shm->size = size;
    shm->is_owner = 1;

    /* Create shared memory object */
    shm->fd = shm_open(shm->name, O_CREAT | O_RDWR | O_EXCL, 0666);
    if (shm->fd == -1) {
        if (errno == EEXIST) {
            /* Already exists, try to open and resize */
            shm->fd = shm_open(shm->name, O_RDWR, 0666);
            if (shm->fd == -1) {
                free(shm);
                return NULL;
            }
        } else {
            free(shm);
            return NULL;
        }
    }

    /* Set size */
    if (ftruncate(shm->fd, (off_t)size) == -1) {
        close(shm->fd);
        shm_unlink(shm->name);
        free(shm);
        return NULL;
    }

    /* Map to memory */
    shm->addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);
    if (shm->addr == MAP_FAILED) {
        close(shm->fd);
        shm_unlink(shm->name);
        free(shm);
        return NULL;
    }

    /* Initialize to zero */
    memset(shm->addr, 0, size);

    return shm;
}

VenomShm* venom_shm_open(const char *name) {
    if (!name) {
        return NULL;
    }

    VenomShm *shm = (VenomShm*)calloc(1, sizeof(VenomShm));
    if (!shm) {
        return NULL;
    }

    build_shm_name(shm->name, sizeof(shm->name), name);
    shm->is_owner = 0;

    /* Open existing shared memory */
    shm->fd = shm_open(shm->name, O_RDWR, 0666);
    if (shm->fd == -1) {
        free(shm);
        return NULL;
    }

    /* Get size */
    struct stat st;
    if (fstat(shm->fd, &st) == -1) {
        close(shm->fd);
        free(shm);
        return NULL;
    }
    shm->size = (size_t)st.st_size;

    /* Map to memory */
    shm->addr = mmap(NULL, shm->size, PROT_READ | PROT_WRITE, MAP_SHARED, shm->fd, 0);
    if (shm->addr == MAP_FAILED) {
        close(shm->fd);
        free(shm);
        return NULL;
    }

    return shm;
}

void venom_shm_close(VenomShm *shm) {
    if (!shm) {
        return;
    }

    if (shm->addr && shm->addr != MAP_FAILED) {
        munmap(shm->addr, shm->size);
    }

    if (shm->fd >= 0) {
        close(shm->fd);
    }

    free(shm);
}

int venom_shm_unlink(const char *name) {
    if (!name) {
        return VENOM_ERR_INVALID;
    }

    char full_name[256];
    build_shm_name(full_name, sizeof(full_name), name);

    if (shm_unlink(full_name) == -1) {
        if (errno == ENOENT) {
            return VENOM_ERR_NOTFOUND;
        }
        return VENOM_ERR_INVALID;
    }

    return VENOM_OK;
}
