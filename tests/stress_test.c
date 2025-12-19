/*
 * VenomMemory Benchmark - Stress Test
 * 
 * Tests with 100x larger message sizes to find limits.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "venom_memory.h"

#define NUM_ITERATIONS 1000
#define WARMUP_ITERATIONS 100

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
    printf("Benchmarking with %zu bytes (%.2f KB / %.2f MB)...\n", 
           message_size, 
           (double)message_size / 1024.0,
           (double)message_size / (1024.0 * 1024.0));
    
    /* Create channel with appropriate buffer sizes */
    size_t buffer_size = message_size + 1024;
    VenomChannelConfig config = {
        .command_buffer_size = buffer_size,
        .response_buffer_size = buffer_size,
        .default_timeout_ms = 30000  /* 30 second timeout for large transfers */
    };
    
    VenomChannel *daemon_ch = venom_channel_create("stress_test", &config);
    if (!daemon_ch) {
        fprintf(stderr, "  FAILED: Could not create daemon channel\n\n");
        return;
    }
    
    /* Start daemon thread */
    daemon_running = 1;
    pthread_t daemon_tid;
    pthread_create(&daemon_tid, NULL, daemon_thread, daemon_ch);
    usleep(10000);  /* Let daemon start */
    
    /* Connect as shell */
    VenomChannel *shell_ch = venom_channel_connect("stress_test");
    if (!shell_ch) {
        fprintf(stderr, "  FAILED: Could not connect\n\n");
        daemon_running = 0;
        pthread_join(daemon_tid, NULL);
        venom_channel_destroy(daemon_ch);
        return;
    }
    
    /* Prepare test data */
    char *send_buf = (char*)malloc(message_size);
    char *recv_buf = (char*)malloc(buffer_size);
    if (!send_buf || !recv_buf) {
        fprintf(stderr, "  FAILED: Could not allocate buffers\n\n");
        free(send_buf);
        free(recv_buf);
        venom_channel_disconnect(shell_ch);
        daemon_running = 0;
        pthread_join(daemon_tid, NULL);
        venom_channel_destroy(daemon_ch);
        return;
    }
    
    /* Fill with pattern for verification */
    for (size_t i = 0; i < message_size; i++) {
        send_buf[i] = (char)((i * 7 + 13) & 0xFF);
    }
    
    /* Warmup */
    printf("  Warming up (%d iterations)...\n", WARMUP_ITERATIONS);
    int warmup_errors = 0;
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        size_t recv_size = buffer_size;
        int result = venom_request(shell_ch, send_buf, message_size, 
                                   recv_buf, &recv_size, 30000);
        if (result != 1) {
            warmup_errors++;
        }
    }
    if (warmup_errors > 0) {
        printf("  WARNING: %d warmup errors\n", warmup_errors);
    }
    
    /* Benchmark */
    printf("  Running %d iterations...\n", NUM_ITERATIONS);
    uint64_t total_ns = 0;
    uint64_t min_ns = UINT64_MAX;
    uint64_t max_ns = 0;
    int errors = 0;
    int timeouts = 0;
    int verification_errors = 0;
    
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        size_t recv_size = buffer_size;
        
        uint64_t start = get_time_ns();
        int result = venom_request(shell_ch, send_buf, message_size, 
                                   recv_buf, &recv_size, 30000);
        uint64_t end = get_time_ns();
        
        if (result != 1) {
            if (result == 0) {
                timeouts++;
            } else {
                errors++;
            }
            continue;
        }
        
        /* Verify data integrity */
        if (recv_size != message_size) {
            verification_errors++;
        } else {
            int ok = 1;
            for (size_t j = 0; j < message_size && ok; j += 1024) {
                if (recv_buf[j] != send_buf[j]) {
                    ok = 0;
                }
            }
            if (!ok) {
                verification_errors++;
            }
        }
        
        uint64_t latency = end - start;
        total_ns += latency;
        if (latency < min_ns) min_ns = latency;
        if (latency > max_ns) max_ns = latency;
    }
    
    /* Results */
    int successful = NUM_ITERATIONS - errors - timeouts;
    if (successful > 0) {
        double avg_us = (double)total_ns / successful / 1000.0;
        double min_us = (double)min_ns / 1000.0;
        double max_us = (double)max_ns / 1000.0;
        double throughput = (double)successful / ((double)total_ns / 1e9);
        double bandwidth_mbps = (double)message_size * successful * 2 / ((double)total_ns / 1e9) / (1024.0 * 1024.0);
        
        printf("  ✓ Successful:   %d/%d\n", successful, NUM_ITERATIONS);
        printf("  ✗ Errors:       %d\n", errors);
        printf("  ⏱ Timeouts:     %d\n", timeouts);
        printf("  ⚠ Data errors:  %d\n", verification_errors);
        printf("  Avg latency:    %.2f µs (%.2f ms)\n", avg_us, avg_us / 1000.0);
        printf("  Min latency:    %.2f µs\n", min_us);
        printf("  Max latency:    %.2f µs (%.2f ms)\n", max_us, max_us / 1000.0);
        printf("  Throughput:     %.0f req/s\n", throughput);
        printf("  Bandwidth:      %.2f MB/s (bidirectional)\n", bandwidth_mbps);
    } else {
        printf("  ✗ All requests failed!\n");
        printf("    Errors: %d, Timeouts: %d\n", errors, timeouts);
    }
    printf("\n");
    
    /* Cleanup */
    free(send_buf);
    free(recv_buf);
    
    venom_channel_disconnect(shell_ch);
    daemon_running = 0;
    
    /* Send one more request to wake up daemon */
    VenomChannel *wake_ch = venom_channel_connect("stress_test");
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
    printf("=== VenomMemory Stress Test (100x sizes) ===\n\n");
    
    /* Original sizes * 100 */
    run_benchmark(64 * 100);         /* 6.4 KB */
    run_benchmark(1024 * 100);       /* 100 KB */
    run_benchmark(4096 * 100);       /* 400 KB */
    run_benchmark(16384 * 100);      /* 1.6 MB */
    
    /* Extra large tests */
    run_benchmark(1024 * 1024);      /* 1 MB */
    run_benchmark(4 * 1024 * 1024);  /* 4 MB */
    run_benchmark(16 * 1024 * 1024); /* 16 MB */
    
    printf("=== Stress Test Complete ===\n");
    return 0;
}
