/*
 * VenomMemory - High-Performance Shared Memory IPC Library
 * 
 * venom_channel.c - Channel implementation
 */

#define _GNU_SOURCE
#include "venom_channel.h"
#include "venom_shm.h"
#include "venom_sync.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* Default configuration values */
#define DEFAULT_CMD_SIZE     (4 * 1024)       /* 4KB */
#define DEFAULT_RESP_SIZE    (64 * 1024)      /* 64KB */
#define DEFAULT_TIMEOUT_MS   5000             /* 5 seconds */

VenomChannelConfig venom_config_default(void) {
    VenomChannelConfig config = {
        .command_buffer_size = DEFAULT_CMD_SIZE,
        .response_buffer_size = DEFAULT_RESP_SIZE,
        .default_timeout_ms = DEFAULT_TIMEOUT_MS
    };
    return config;
}

/* Calculate required shared memory size */
static size_t calculate_shm_size(const VenomChannelConfig *config) {
    return sizeof(VenomChannelHeader) + 
           config->command_buffer_size + 
           config->response_buffer_size;
}

/* ============================================
 * Daemon API Implementation
 * ============================================ */

VenomChannel* venom_channel_create(const char *namespace, 
                                   const VenomChannelConfig *config) {
    if (!namespace || strlen(namespace) == 0) {
        return NULL;
    }

    /* Use default config if not provided */
    VenomChannelConfig cfg;
    if (config) {
        cfg = *config;
    } else {
        cfg = venom_config_default();
    }

    /* Validate configuration */
    if (cfg.command_buffer_size == 0 || cfg.response_buffer_size == 0) {
        return NULL;
    }

    /* Allocate channel structure */
    VenomChannel *channel = (VenomChannel*)calloc(1, sizeof(VenomChannel));
    if (!channel) {
        return NULL;
    }

    strncpy(channel->namespace, namespace, sizeof(channel->namespace) - 1);
    channel->is_daemon = 1;

    /* Calculate and create shared memory */
    size_t shm_size = calculate_shm_size(&cfg);
    channel->shm = venom_shm_create(namespace, shm_size);
    if (!channel->shm) {
        free(channel);
        return NULL;
    }

    /* Set up header */
    channel->header = (VenomChannelHeader*)venom_shm_addr(channel->shm);
    channel->header->magic = VENOM_MAGIC;
    channel->header->version = VENOM_VERSION;
    channel->header->total_size = shm_size;
    channel->header->cmd_buffer_size = cfg.command_buffer_size;
    channel->header->resp_buffer_size = cfg.response_buffer_size;
    channel->header->cmd_offset = sizeof(VenomChannelHeader);
    channel->header->resp_offset = sizeof(VenomChannelHeader) + cfg.command_buffer_size;
    atomic_store(&channel->header->state, VENOM_STATE_IDLE);
    atomic_store(&channel->header->cmd_data_size, 0);
    atomic_store(&channel->header->resp_data_size, 0);

    /* Set up buffer pointers */
    uint8_t *base = (uint8_t*)channel->header;
    channel->cmd_buffer = base + channel->header->cmd_offset;
    channel->resp_buffer = base + channel->header->resp_offset;

    return channel;
}

int venom_channel_listen(VenomChannel *channel,
                         VenomCommandHandler handler,
                         void *userdata) {
    if (!channel || !channel->is_daemon || !handler) {
        return -1;
    }

    while (1) {
        /* Wait for command state using futex on shared memory */
        uint32_t current_state = atomic_load(&channel->header->state);
        
        while (current_state != VENOM_STATE_CMD_READY) {
            /* Wait for state to change from current value */
            int ret = venom_futex_wait(&channel->header->state, current_state, VENOM_WAIT_FOREVER);
            if (ret == -1) {
                return -1;  /* Error */
            }
            current_state = atomic_load(&channel->header->state);
        }

        /* Mark as executing */
        atomic_store(&channel->header->state, VENOM_STATE_EXECUTING);
        venom_futex_wake(&channel->header->state);

        /* Get command data */
        size_t cmd_size = atomic_load(&channel->header->cmd_data_size);

        /* Call handler */
        int result = handler(channel, channel->cmd_buffer, cmd_size, userdata);

        /* If handler didn't send response, send empty one */
        uint32_t state = atomic_load(&channel->header->state);
        if (state == VENOM_STATE_EXECUTING) {
            venom_send_response(channel, NULL, 0);
        }

        if (result != 0) {
            return 0;  /* Handler requested stop */
        }
    }

    return 0;
}

int venom_send_response(VenomChannel *channel, 
                        const void *data, 
                        size_t size) {
    if (!channel || !channel->is_daemon) {
        return -1;
    }

    /* Validate size */
    if (size > channel->header->resp_buffer_size) {
        return -1;
    }

    /* Copy response data */
    if (data && size > 0) {
        memcpy(channel->resp_buffer, data, size);
    }
    atomic_store(&channel->header->resp_data_size, size);

    /* Update state and wake shell */
    atomic_store(&channel->header->state, VENOM_STATE_RESPONSE_READY);
    venom_futex_wake(&channel->header->state);

    return 0;
}

int venom_send_error(VenomChannel *channel, const char *error_msg) {
    if (!channel || !channel->is_daemon) {
        return -1;
    }

    /* Store error message as response */
    size_t len = error_msg ? strlen(error_msg) : 0;
    if (len > channel->header->resp_buffer_size) {
        len = channel->header->resp_buffer_size - 1;
    }

    if (len > 0) {
        memcpy(channel->resp_buffer, error_msg, len);
        ((char*)channel->resp_buffer)[len] = '\0';
    }
    atomic_store(&channel->header->resp_data_size, len);

    /* Update state to error and wake shell */
    atomic_store(&channel->header->state, VENOM_STATE_ERROR);
    venom_futex_wake(&channel->header->state);

    return 0;
}

void venom_channel_destroy(VenomChannel *channel) {
    if (!channel) {
        return;
    }

    /* Only daemon should unlink shared memory */
    if (channel->is_daemon) {
        venom_shm_unlink(channel->namespace);
    }

    if (channel->shm) {
        venom_shm_close(channel->shm);
    }

    free(channel);
}

/* ============================================
 * Shell API Implementation
 * ============================================ */

VenomChannel* venom_channel_connect(const char *namespace) {
    if (!namespace || strlen(namespace) == 0) {
        return NULL;
    }

    /* Allocate channel structure */
    VenomChannel *channel = (VenomChannel*)calloc(1, sizeof(VenomChannel));
    if (!channel) {
        return NULL;
    }

    strncpy(channel->namespace, namespace, sizeof(channel->namespace) - 1);
    channel->is_daemon = 0;

    /* Open existing shared memory */
    channel->shm = venom_shm_open(namespace);
    if (!channel->shm) {
        free(channel);
        return NULL;
    }

    /* Validate header */
    channel->header = (VenomChannelHeader*)venom_shm_addr(channel->shm);
    if (channel->header->magic != VENOM_MAGIC) {
        venom_shm_close(channel->shm);
        free(channel);
        return NULL;
    }

    /* Set up buffer pointers */
    uint8_t *base = (uint8_t*)channel->header;
    channel->cmd_buffer = base + channel->header->cmd_offset;
    channel->resp_buffer = base + channel->header->resp_offset;

    return channel;
}

int venom_send_command(VenomChannel *channel, 
                       const void *cmd, 
                       size_t cmd_size) {
    if (!channel || channel->is_daemon) {
        return -1;
    }

    /* Validate size */
    if (cmd_size > channel->header->cmd_buffer_size) {
        return -1;
    }

    /* Wait for idle state */
    uint32_t state = atomic_load(&channel->header->state);
    if (state != VENOM_STATE_IDLE) {
        return -1;  /* Channel busy */
    }

    /* Copy command data */
    if (cmd && cmd_size > 0) {
        memcpy(channel->cmd_buffer, cmd, cmd_size);
    }
    atomic_store(&channel->header->cmd_data_size, cmd_size);

    /* Update state and wake daemon */
    atomic_store(&channel->header->state, VENOM_STATE_CMD_READY);
    venom_futex_wake(&channel->header->state);

    return 0;
}

int venom_wait_response(VenomChannel *channel,
                        void *buffer,
                        size_t buffer_size,
                        size_t *received,
                        int timeout_ms) {
    if (!channel || channel->is_daemon) {
        return -1;
    }

    /* Wait for response state using futex */
    uint32_t current_state = atomic_load(&channel->header->state);
    
    while (current_state != VENOM_STATE_RESPONSE_READY && 
           current_state != VENOM_STATE_ERROR) {
        int ret = venom_futex_wait(&channel->header->state, current_state, timeout_ms);
        if (ret == -2) {
            return 0;  /* Timeout */
        }
        if (ret == -1) {
            return -1;  /* Error */
        }
        current_state = atomic_load(&channel->header->state);
    }

    /* Copy response */
    size_t resp_size = atomic_load(&channel->header->resp_data_size);
    size_t copy_size = (resp_size < buffer_size) ? resp_size : buffer_size;
    
    if (buffer && copy_size > 0) {
        memcpy(buffer, channel->resp_buffer, copy_size);
    }
    
    if (received) {
        *received = resp_size;
    }

    int was_error = (current_state == VENOM_STATE_ERROR);

    /* Reset state to idle */
    atomic_store(&channel->header->state, VENOM_STATE_IDLE);
    venom_futex_wake(&channel->header->state);

    return was_error ? -1 : 1;
}

int venom_request(VenomChannel *channel,
                  const void *cmd,
                  size_t cmd_size,
                  void *resp_buffer,
                  size_t *resp_size,
                  int timeout_ms) {
    int ret = venom_send_command(channel, cmd, cmd_size);
    if (ret < 0) {
        return ret;
    }

    size_t buffer_size = resp_size ? *resp_size : 0;
    return venom_wait_response(channel, resp_buffer, buffer_size, resp_size, timeout_ms);
}

void venom_channel_disconnect(VenomChannel *channel) {
    if (!channel || channel->is_daemon) {
        return;
    }

    if (channel->shm) {
        venom_shm_close(channel->shm);
    }

    free(channel);
}

/* ============================================
 * Utility Functions
 * ============================================ */

size_t venom_channel_max_cmd_size(VenomChannel *channel) {
    return channel ? channel->header->cmd_buffer_size : 0;
}

size_t venom_channel_max_response_size(VenomChannel *channel) {
    return channel ? channel->header->resp_buffer_size : 0;
}

VenomState venom_channel_state(VenomChannel *channel) {
    if (!channel) {
        return VENOM_STATE_ERROR;
    }
    return (VenomState)atomic_load(&channel->header->state);
}

int venom_channel_is_valid(VenomChannel *channel) {
    return channel && channel->shm && channel->header &&
           channel->header->magic == VENOM_MAGIC;
}
