/*
 * VenomMemory Example - Daemon (Server)
 * 
 * This example shows how to create a daemon that listens for
 * commands and sends responses through shared memory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include "venom_memory.h"

/* Global channel for signal handler */
static VenomChannel *g_channel = NULL;
static volatile int g_running = 1;

/* Signal handler for graceful shutdown */
void signal_handler(int sig) {
    (void)sig;
    g_running = 0;
    printf("\n[Daemon] Shutting down...\n");
}

/* Command handler callback */
int handle_command(VenomChannel *channel,
                   const void *cmd,
                   size_t cmd_size,
                   void *userdata) {
    (void)userdata;
    
    /* Null-terminate the command for safe printing */
    char cmd_str[1024];
    size_t copy_len = cmd_size < sizeof(cmd_str) - 1 ? cmd_size : sizeof(cmd_str) - 1;
    memcpy(cmd_str, cmd, copy_len);
    cmd_str[copy_len] = '\0';

    printf("[Daemon] Received command: %s (size: %zu)\n", cmd_str, cmd_size);

    /* Process command */
    char response[1024];
    
    if (strcmp(cmd_str, "ping") == 0) {
        snprintf(response, sizeof(response), "pong");
    }
    else if (strcmp(cmd_str, "time") == 0) {
        snprintf(response, sizeof(response), "Unix time: %ld", (long)time(NULL));
    }
    else if (strcmp(cmd_str, "pid") == 0) {
        snprintf(response, sizeof(response), "Daemon PID: %d", getpid());
    }
    else if (strcmp(cmd_str, "quit") == 0) {
        snprintf(response, sizeof(response), "Goodbye!");
        venom_send_response(channel, response, strlen(response));
        return 1;  /* Stop listening */
    }
    else {
        snprintf(response, sizeof(response), "Unknown command: %s", cmd_str);
    }

    /* Send response */
    printf("[Daemon] Sending response: %s\n", response);
    venom_send_response(channel, response, strlen(response));

    return g_running ? 0 : 1;
}

int main(int argc, char *argv[]) {
    const char *namespace = "example_daemon";
    
    if (argc > 1) {
        namespace = argv[1];
    }

    printf("[Daemon] Starting with namespace: %s\n", namespace);

    /* Set up signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Configure channel with custom buffer sizes */
    VenomChannelConfig config = {
        .command_buffer_size = 4096,      /* 4KB for commands */
        .response_buffer_size = 65536,    /* 64KB for responses */
        .default_timeout_ms = 0           /* No timeout */
    };

    /* Create channel */
    g_channel = venom_channel_create(namespace, &config);
    if (!g_channel) {
        fprintf(stderr, "[Daemon] Failed to create channel\n");
        return 1;
    }

    printf("[Daemon] Channel created successfully\n");
    printf("[Daemon] Command buffer: %zu bytes\n", 
           venom_channel_max_cmd_size(g_channel));
    printf("[Daemon] Response buffer: %zu bytes\n", 
           venom_channel_max_response_size(g_channel));
    printf("[Daemon] Listening for commands...\n");

    /* Start listening (blocking) */
    int result = venom_channel_listen(g_channel, handle_command, NULL);

    printf("[Daemon] Stopped listening (result: %d)\n", result);

    /* Cleanup */
    venom_channel_destroy(g_channel);
    printf("[Daemon] Channel destroyed\n");

    return 0;
}
