/*
 * VenomMemory Example - Shell (Client)
 * 
 * This example shows how to connect to a daemon and
 * send commands / receive responses through shared memory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "venom_memory.h"

void print_usage(const char *prog) {
    printf("Usage: %s [namespace] [command]\n", prog);
    printf("\n");
    printf("If no command is provided, enters interactive mode.\n");
    printf("\n");
    printf("Available commands:\n");
    printf("  ping   - Test connection\n");
    printf("  time   - Get Unix timestamp\n");
    printf("  pid    - Get daemon PID\n");
    printf("  quit   - Stop the daemon\n");
}

int send_and_print(VenomChannel *channel, const char *cmd) {
    char response[4096];
    size_t resp_size = sizeof(response);

    printf("[Shell] Sending: %s\n", cmd);
    
    int result = venom_request(channel, 
                               cmd, 
                               strlen(cmd), 
                               response, 
                               &resp_size,
                               5000);  /* 5 second timeout */

    if (result < 0) {
        fprintf(stderr, "[Shell] Error sending command\n");
        return -1;
    }
    else if (result == 0) {
        fprintf(stderr, "[Shell] Timeout waiting for response\n");
        return -1;
    }
    else {
        /* Null-terminate response */
        if (resp_size < sizeof(response)) {
            response[resp_size] = '\0';
        } else {
            response[sizeof(response) - 1] = '\0';
        }
        printf("[Shell] Response: %s\n", response);
        return 0;
    }
}

int interactive_mode(VenomChannel *channel) {
    char input[1024];
    
    printf("[Shell] Interactive mode. Type 'exit' to quit.\n");
    printf("\n");

    while (1) {
        printf(">>> ");
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        /* Remove newline */
        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
            len--;
        }

        /* Skip empty input */
        if (len == 0) {
            continue;
        }

        /* Check for exit command */
        if (strcmp(input, "exit") == 0) {
            printf("[Shell] Exiting...\n");
            break;
        }

        /* Send command */
        send_and_print(channel, input);
        printf("\n");
    }

    return 0;
}

int main(int argc, char *argv[]) {
    const char *namespace = "example_daemon";
    const char *command = NULL;

    /* Parse arguments */
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
        namespace = argv[1];
    }
    if (argc > 2) {
        command = argv[2];
    }

    printf("[Shell] Connecting to namespace: %s\n", namespace);

    /* Connect to daemon */
    VenomChannel *channel = venom_channel_connect(namespace);
    if (!channel) {
        fprintf(stderr, "[Shell] Failed to connect to daemon\n");
        fprintf(stderr, "[Shell] Make sure the daemon is running first!\n");
        return 1;
    }

    printf("[Shell] Connected successfully\n");
    printf("[Shell] Max command size: %zu bytes\n", 
           venom_channel_max_cmd_size(channel));
    printf("[Shell] Max response size: %zu bytes\n", 
           venom_channel_max_response_size(channel));
    printf("\n");

    int result;
    if (command) {
        /* Single command mode */
        result = send_and_print(channel, command);
    } else {
        /* Interactive mode */
        result = interactive_mode(channel);
    }

    /* Disconnect */
    venom_channel_disconnect(channel);
    printf("[Shell] Disconnected\n");

    return result;
}
