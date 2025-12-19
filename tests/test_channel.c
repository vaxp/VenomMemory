/*
 * VenomMemory Test - Channel
 * 
 * Tests for channel creation and communication.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "venom_memory.h"

#define TEST(name) printf("Testing: %s... ", name)
#define PASS() printf("PASSED\n")
#define FAIL(msg) do { printf("FAILED: %s\n", msg); failures++; } while(0)

static int failures = 0;

void test_channel_create(void) {
    TEST("channel_create with default config");
    
    VenomChannel *ch = venom_channel_create("test_create", NULL);
    if (!ch) {
        FAIL("Failed to create channel");
        return;
    }
    
    if (!venom_channel_is_valid(ch)) {
        FAIL("Channel not valid");
        venom_channel_destroy(ch);
        return;
    }
    
    venom_channel_destroy(ch);
    PASS();
}

void test_channel_custom_config(void) {
    TEST("channel with custom buffer sizes");
    
    VenomChannelConfig config = {
        .command_buffer_size = 8192,
        .response_buffer_size = 128 * 1024,  /* 128KB */
        .default_timeout_ms = 1000
    };
    
    VenomChannel *ch = venom_channel_create("test_custom", &config);
    if (!ch) {
        FAIL("Failed to create channel");
        return;
    }
    
    if (venom_channel_max_cmd_size(ch) != 8192) {
        FAIL("Command buffer size mismatch");
        venom_channel_destroy(ch);
        return;
    }
    
    if (venom_channel_max_response_size(ch) != 128 * 1024) {
        FAIL("Response buffer size mismatch");
        venom_channel_destroy(ch);
        return;
    }
    
    venom_channel_destroy(ch);
    PASS();
}

void test_channel_connect(void) {
    TEST("channel connect from shell");
    
    /* Create daemon channel */
    VenomChannel *daemon_ch = venom_channel_create("test_connect", NULL);
    if (!daemon_ch) {
        FAIL("Failed to create daemon channel");
        return;
    }
    
    /* Connect as shell */
    VenomChannel *shell_ch = venom_channel_connect("test_connect");
    if (!shell_ch) {
        FAIL("Failed to connect as shell");
        venom_channel_destroy(daemon_ch);
        return;
    }
    
    /* Verify both see same configuration */
    if (venom_channel_max_cmd_size(shell_ch) != 
        venom_channel_max_cmd_size(daemon_ch)) {
        FAIL("Buffer size mismatch between daemon and shell");
        venom_channel_disconnect(shell_ch);
        venom_channel_destroy(daemon_ch);
        return;
    }
    
    venom_channel_disconnect(shell_ch);
    venom_channel_destroy(daemon_ch);
    PASS();
}

/* Thread data for communication test */
struct comm_test_data {
    VenomChannel *channel;
    int success;
};

/* Daemon thread for communication test */
void* daemon_thread(void *arg) {
    struct comm_test_data *data = (struct comm_test_data*)arg;
    
    /* Simple handler - just echo the command */
    int handler(VenomChannel *ch, const void *cmd, size_t size, void *ud) {
        (void)ud;
        char response[256];
        snprintf(response, sizeof(response), "Echo: %.*s", (int)size, (char*)cmd);
        venom_send_response(ch, response, strlen(response));
        return 1;  /* Stop after one command */
    }
    
    venom_channel_listen(data->channel, handler, NULL);
    return NULL;
}

void test_channel_communication(void) {
    TEST("channel request/response");
    
    /* Create daemon channel */
    VenomChannel *daemon_ch = venom_channel_create("test_comm", NULL);
    if (!daemon_ch) {
        FAIL("Failed to create daemon channel");
        return;
    }
    
    /* Start daemon in thread */
    struct comm_test_data daemon_data = { .channel = daemon_ch, .success = 0 };
    pthread_t daemon_tid;
    pthread_create(&daemon_tid, NULL, daemon_thread, &daemon_data);
    
    /* Give daemon time to start listening */
    usleep(10000);  /* 10ms */
    
    /* Connect as shell */
    VenomChannel *shell_ch = venom_channel_connect("test_comm");
    if (!shell_ch) {
        FAIL("Failed to connect as shell");
        pthread_join(daemon_tid, NULL);
        venom_channel_destroy(daemon_ch);
        return;
    }
    
    /* Send command and wait for response */
    const char *cmd = "Hello";
    char response[256];
    size_t resp_size = sizeof(response);
    
    int result = venom_request(shell_ch, cmd, strlen(cmd), 
                               response, &resp_size, 1000);
    
    if (result != 1) {
        FAIL("Request failed or timed out");
        venom_channel_disconnect(shell_ch);
        pthread_join(daemon_tid, NULL);
        venom_channel_destroy(daemon_ch);
        return;
    }
    
    /* Verify response */
    response[resp_size] = '\0';
    if (strstr(response, "Hello") == NULL) {
        FAIL("Response doesn't contain original message");
        venom_channel_disconnect(shell_ch);
        pthread_join(daemon_tid, NULL);
        venom_channel_destroy(daemon_ch);
        return;
    }
    
    venom_channel_disconnect(shell_ch);
    pthread_join(daemon_tid, NULL);
    venom_channel_destroy(daemon_ch);
    PASS();
}

int main(void) {
    printf("=== VenomMemory Channel Tests ===\n\n");
    
    test_channel_create();
    test_channel_custom_config();
    test_channel_connect();
    test_channel_communication();
    
    printf("\n=== Results: %d failures ===\n", failures);
    return failures > 0 ? 1 : 0;
}
