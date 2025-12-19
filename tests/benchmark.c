/*
 * VenomMemory Benchmark
 * 
 * Measures round-trip latency for shared memory communication.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "venom_memory.h"

#define NUM_ITERATIONS 10000
#define WARMUP_ITERATIONS 1000

/* Get current time in nanoseconds */
static inline uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* Daemon thread - echoes commands */
static volatile int daemon_running = 1;

void* daemon_thread(void *arg) {
    VenomChannel *channel = (VenomChannel*)arg;
    
    int handler(VenomChannel *ch, const void *cmd, size_t size, void *ud) {
        (void)ud;
        /* Echo the command back */
        venom_send_response(ch, cmd, size);
        return daemon_running ? 0 : 1;
    }
    
    venom_channel_listen(channel, handler, NULL);
    return NULL;
}

void run_benchmark(size_t message_size) {
    printf("Benchmarking with %zu byte messages...\n", message_size);
    
    /* Create channel */
    VenomChannelConfig config = {
        .command_buffer_size = message_size + 1024,
        .response_buffer_size = message_size + 1024,
        .default_timeout_ms = 5000
    };
    
    VenomChannel *daemon_ch = venom_channel_create("benchmark", &config);
    if (!daemon_ch) {
        fprintf(stderr, "Failed to create daemon channel\n");
        return;
    }
    
    /* Start daemon thread */
    daemon_running = 1;
    pthread_t daemon_tid;
    pthread_create(&daemon_tid, NULL, daemon_thread, daemon_ch);
    usleep(10000);  /* Let daemon start */
    
    /* Connect as shell */
    VenomChannel *shell_ch = venom_channel_connect("benchmark");
    if (!shell_ch) {
        fprintf(stderr, "Failed to connect\n");
        daemon_running = 0;
        pthread_join(daemon_tid, NULL);
        venom_channel_destroy(daemon_ch);
        return;
    }
    
    /* Prepare test data */
    char *send_buf = (char*)malloc(message_size);
    char *recv_buf = (char*)malloc(message_size + 1024);
    memset(send_buf, 'A', message_size);
    
    /* Warmup */
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        size_t recv_size = message_size + 1024;
        venom_request(shell_ch, send_buf, message_size, 
                      recv_buf, &recv_size, 1000);
    }
    
    /* Benchmark */
    uint64_t total_ns = 0;
    uint64_t min_ns = UINT64_MAX;
    uint64_t max_ns = 0;
    
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        size_t recv_size = message_size + 1024;
        
        uint64_t start = get_time_ns();
        int result = venom_request(shell_ch, send_buf, message_size, 
                                   recv_buf, &recv_size, 1000);
        uint64_t end = get_time_ns();
        
        if (result != 1) {
            fprintf(stderr, "Request failed at iteration %d\n", i);
            break;
        }
        
        uint64_t latency = end - start;
        total_ns += latency;
        if (latency < min_ns) min_ns = latency;
        if (latency > max_ns) max_ns = latency;
    }
    
    /* Results */
    double avg_us = (double)total_ns / NUM_ITERATIONS / 1000.0;
    double min_us = (double)min_ns / 1000.0;
    double max_us = (double)max_ns / 1000.0;
    double throughput = (double)NUM_ITERATIONS / ((double)total_ns / 1e9);
    
    printf("  Iterations:  %d\n", NUM_ITERATIONS);
    printf("  Avg latency: %.2f µs\n", avg_us);
    printf("  Min latency: %.2f µs\n", min_us);
    printf("  Max latency: %.2f µs\n", max_us);
    printf("  Throughput:  %.0f req/s\n", throughput);
    printf("\n");
    
    /* Cleanup */
    free(send_buf);
    free(recv_buf);
    
    venom_channel_disconnect(shell_ch);
    daemon_running = 0;
    
    /* Send one more request to wake up daemon */
    VenomChannel *wake_ch = venom_channel_connect("benchmark");
    if (wake_ch) {
        char dummy = 'x';
        size_t dummy_size = 1;
        venom_request(wake_ch, &dummy, 1, &dummy, &dummy_size, 100);
        venom_channel_disconnect(wake_ch);
    }
    
    pthread_join(daemon_tid, NULL);
    venom_channel_destroy(daemon_ch);
}

int main(void) {
    printf("=== VenomMemory Benchmark ===\n\n");
    
    /* Test different message sizes */
    run_benchmark(64);      /* Small message */
    run_benchmark(1024);    /* 1KB */
    run_benchmark(4096);    /* 4KB */
    run_benchmark(16384);   /* 16KB */
    
    printf("=== Benchmark Complete ===\n");
    return 0;
}
