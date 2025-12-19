/*
 * VenomMemory - High-Performance Shared Memory IPC Library
 * 
 * venom_channel.h - Channel abstraction for daemon/shell communication
 */

#ifndef VENOM_CHANNEL_H
#define VENOM_CHANNEL_H

#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct VenomShm;
struct VenomEvent;

/* ============================================
 * Configuration
 * ============================================ */

/**
 * Channel configuration - daemon specifies buffer sizes
 */
typedef struct VenomChannelConfig {
    size_t command_buffer_size;    /* Size of command buffer (bytes) */
    size_t response_buffer_size;   /* Size of response buffer (bytes) */
    int default_timeout_ms;        /* Default timeout (0 = infinite) */
} VenomChannelConfig;

/**
 * Get default configuration
 * Command: 4KB, Response: 64KB, Timeout: 5000ms
 */
VenomChannelConfig venom_config_default(void);

/* ============================================
 * Channel States
 * ============================================ */

typedef enum VenomState {
    VENOM_STATE_IDLE = 0,          /* Ready for new command */
    VENOM_STATE_CMD_READY,         /* Command written, waiting for daemon */
    VENOM_STATE_EXECUTING,         /* Daemon is processing */
    VENOM_STATE_RESPONSE_READY,    /* Response written, waiting for shell */
    VENOM_STATE_ERROR              /* Error occurred */
} VenomState;

/* ============================================
 * Channel Header (stored in shared memory)
 * ============================================ */

#define VENOM_MAGIC 0x564E4F4D  /* "VNOM" */
#define VENOM_VERSION 1

/**
 * Header structure at the beginning of shared memory
 */
typedef struct VenomChannelHeader {
    uint32_t magic;                  /* Magic number for validation */
    uint32_t version;                /* Protocol version */
    size_t total_size;               /* Total shared memory size */
    size_t cmd_buffer_size;          /* Command buffer capacity */
    size_t resp_buffer_size;         /* Response buffer capacity */
    size_t cmd_offset;               /* Offset to command buffer */
    size_t resp_offset;              /* Offset to response buffer */
    _Atomic uint32_t state;          /* Current channel state (used for futex) */
    _Atomic size_t cmd_data_size;    /* Actual command data size */
    _Atomic size_t resp_data_size;   /* Actual response data size */
} VenomChannelHeader;

/* ============================================
 * Channel Handle
 * ============================================ */

typedef struct VenomChannel {
    struct VenomShm *shm;            /* Shared memory handle */
    VenomChannelHeader *header;      /* Pointer to header in shm */
    void *cmd_buffer;                /* Pointer to command buffer */
    void *resp_buffer;               /* Pointer to response buffer */
    int is_daemon;                   /* 1 if daemon, 0 if shell */
    char namespace[256];             /* Channel namespace */
} VenomChannel;

/* ============================================
 * Command Handler Callback
 * ============================================ */

/**
 * Callback function for processing commands
 * 
 * @param channel   The channel that received the command
 * @param cmd       Pointer to command data
 * @param cmd_size  Size of command data
 * @param userdata  User-provided data
 * @return          0 to continue, non-zero to stop listening
 */
typedef int (*VenomCommandHandler)(VenomChannel *channel,
                                   const void *cmd,
                                   size_t cmd_size,
                                   void *userdata);

/* ============================================
 * Daemon API
 * ============================================ */

/**
 * Create a new channel (daemon side)
 * 
 * @param namespace  Channel name (unique identifier)
 * @param config     Configuration (NULL for defaults)
 * @return           Channel handle on success, NULL on failure
 */
VenomChannel* venom_channel_create(const char *namespace, 
                                   const VenomChannelConfig *config);

/**
 * Start listening for commands (blocking)
 * 
 * @param channel   Channel handle
 * @param handler   Callback for processing commands
 * @param userdata  User data passed to callback
 * @return          0 on normal exit, -1 on error
 */
int venom_channel_listen(VenomChannel *channel,
                         VenomCommandHandler handler,
                         void *userdata);

/**
 * Send response to shell (called from handler)
 * 
 * @param channel   Channel handle
 * @param data      Response data
 * @param size      Size of response data
 * @return          0 on success, -1 on error
 */
int venom_send_response(VenomChannel *channel, 
                        const void *data, 
                        size_t size);

/**
 * Send error response to shell
 * 
 * @param channel   Channel handle
 * @param error_msg Error message string
 * @return          0 on success, -1 on error
 */
int venom_send_error(VenomChannel *channel, const char *error_msg);

/**
 * Destroy channel and release resources (daemon side)
 * 
 * @param channel   Channel handle
 */
void venom_channel_destroy(VenomChannel *channel);

/* ============================================
 * Shell/Client API
 * ============================================ */

/**
 * Connect to an existing channel (shell side)
 * 
 * @param namespace  Channel name to connect to
 * @return           Channel handle on success, NULL on failure
 */
VenomChannel* venom_channel_connect(const char *namespace);

/**
 * Send command to daemon
 * 
 * @param channel   Channel handle
 * @param cmd       Command data
 * @param cmd_size  Size of command data
 * @return          0 on success, -1 on error
 */
int venom_send_command(VenomChannel *channel, 
                       const void *cmd, 
                       size_t cmd_size);

/**
 * Wait for and read response from daemon
 * 
 * @param channel     Channel handle
 * @param buffer      Buffer to receive response
 * @param buffer_size Size of buffer
 * @param received    Actual size of response (output)
 * @param timeout_ms  Timeout in milliseconds (-1 = infinite)
 * @return            1 on success, 0 on timeout, -1 on error
 */
int venom_wait_response(VenomChannel *channel,
                        void *buffer,
                        size_t buffer_size,
                        size_t *received,
                        int timeout_ms);

/**
 * Send command and wait for response (convenience function)
 * 
 * @param channel      Channel handle
 * @param cmd          Command data
 * @param cmd_size     Size of command
 * @param resp_buffer  Buffer for response
 * @param resp_size    Size of buffer (in), received size (out)
 * @param timeout_ms   Timeout in milliseconds
 * @return             1 on success, 0 on timeout, -1 on error
 */
int venom_request(VenomChannel *channel,
                  const void *cmd,
                  size_t cmd_size,
                  void *resp_buffer,
                  size_t *resp_size,
                  int timeout_ms);

/**
 * Disconnect from channel (shell side)
 * 
 * @param channel   Channel handle
 */
void venom_channel_disconnect(VenomChannel *channel);

/* ============================================
 * Utility Functions
 * ============================================ */

/**
 * Get maximum command size for this channel
 */
size_t venom_channel_max_cmd_size(VenomChannel *channel);

/**
 * Get maximum response size for this channel
 */
size_t venom_channel_max_response_size(VenomChannel *channel);

/**
 * Get current channel state
 */
VenomState venom_channel_state(VenomChannel *channel);

/**
 * Check if channel is valid and connected
 */
int venom_channel_is_valid(VenomChannel *channel);

#ifdef __cplusplus
}
#endif

#endif /* VENOM_CHANNEL_H */
