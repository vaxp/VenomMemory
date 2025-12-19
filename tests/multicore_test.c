/*
 * VenomMemory Benchmark - Multi-Core Stress Test
 * 
 * Tests parallel channels across multiple CPU cores.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>
#include "venom_memory.h"

#define NUM_ITERATIONS 1000000
#define WARMUP_ITERATIONS 1000

/* Get current time in nanoseconds */
static inline uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* Per-channel test results */
typedef struct {
    int channel_id;
    size_t message_size;
    int successful;
    int errors;
    int timeouts;
    int data_errors;
    uint64_t total_ns;
    uint64_t min_ns;
    uint64_t max_ns;
} ChannelResult;

/* Daemon thread arguments */
typedef struct {
    int channel_id;
    char namespace[64];
    size_t buffer_size;
    volatile int *running;
    VenomChannel *channel;
} DaemonArgs;

/* Shell thread arguments */
typedef struct {
    int channel_id;
    char namespace[64];
    size_t message_size;
    ChannelResult *result;
    volatile int *start_signal;
} ShellArgs;

/* Daemon thread - echoes commands */
void* daemon_thread(void *arg) {
    DaemonArgs *args = (DaemonArgs*)arg;
    
    /* Pin to specific core if possible */
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(args->channel_id % sysconf(_SC_NPROCESSORS_ONLN), &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    
    int handler(VenomChannel *ch, const void *cmd, size_t size, void *ud) {
        volatile int *running = (volatile int*)ud;
        venom_send_response(ch, cmd, size);
        return *running ? 0 : 1;
    }
    
    venom_channel_listen(args->channel, handler, (void*)args->running);
    return NULL;
}

/* Shell thread - sends commands */
void* shell_thread(void *arg) {
    ShellArgs *args = (ShellArgs*)arg;
    ChannelResult *result = args->result;
    
    /* Pin to different core than daemon */
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET((args->channel_id + num_cpus/2) % num_cpus, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    
    /* Wait for start signal */
    while (!*args->start_signal) {
        usleep(100);
    }
    
    /* Connect to channel */
    VenomChannel *channel = venom_channel_connect(args->namespace);
    if (!channel) {
        result->errors = NUM_ITERATIONS;
        return NULL;
    }
    
    size_t buffer_size = args->message_size + 1024;
    char *send_buf = malloc(args->message_size);
    char *recv_buf = malloc(buffer_size);
    
    if (!send_buf || !recv_buf) {
        result->errors = NUM_ITERATIONS;
        free(send_buf);
        free(recv_buf);
        venom_channel_disconnect(channel);
        return NULL;
    }
    
    /* Fill with pattern */
    for (size_t i = 0; i < args->message_size; i++) {
        send_buf[i] = (char)((i * 7 + args->channel_id) & 0xFF);
    }
    
    /* Initialize result */
    result->channel_id = args->channel_id;
    result->message_size = args->message_size;
    result->successful = 0;
    result->errors = 0;
    result->timeouts = 0;
    result->data_errors = 0;
    result->total_ns = 0;
    result->min_ns = UINT64_MAX;
    result->max_ns = 0;
    
    /* Warmup */
    for (int i = 0; i < WARMUP_ITERATIONS; i++) {
        size_t recv_size = buffer_size;
        venom_request(channel, send_buf, args->message_size, 
                      recv_buf, &recv_size, 10000);
    }
    
    /* Benchmark */
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        size_t recv_size = buffer_size;
        
        uint64_t start = get_time_ns();
        int ret = venom_request(channel, send_buf, args->message_size, 
                                recv_buf, &recv_size, 10000);
        uint64_t end = get_time_ns();
        
        if (ret != 1) {
            if (ret == 0) result->timeouts++;
            else result->errors++;
            continue;
        }
        
        /* Verify */
        if (recv_size != args->message_size || 
            memcmp(recv_buf, send_buf, args->message_size) != 0) {
            result->data_errors++;
        }
        
        uint64_t latency = end - start;
        result->total_ns += latency;
        result->successful++;
        if (latency < result->min_ns) result->min_ns = latency;
        if (latency > result->max_ns) result->max_ns = latency;
    }
    
    free(send_buf);
    free(recv_buf);
    venom_channel_disconnect(channel);
    
    return NULL;
}

void run_multicore_test(int num_channels, size_t message_size) {
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Testing: %d parallel channels, %zu bytes (%.2f KB)\n", 
           num_channels, message_size, (double)message_size / 1024.0);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    size_t buffer_size = message_size + 1024;
    VenomChannelConfig config = {
        .command_buffer_size = buffer_size,
        .response_buffer_size = buffer_size,
        .default_timeout_ms = 10000
    };
    
    /* Allocate arrays */
    DaemonArgs *daemon_args = calloc(num_channels, sizeof(DaemonArgs));
    ShellArgs *shell_args = calloc(num_channels, sizeof(ShellArgs));
    ChannelResult *results = calloc(num_channels, sizeof(ChannelResult));
    pthread_t *daemon_threads = calloc(num_channels, sizeof(pthread_t));
    pthread_t *shell_threads = calloc(num_channels, sizeof(pthread_t));
    volatile int *running = calloc(num_channels, sizeof(int));
    volatile int start_signal = 0;
    
    /* Create channels and start daemon threads */
    printf("Creating %d daemon channels...\n", num_channels);
    for (int i = 0; i < num_channels; i++) {
        running[i] = 1;
        snprintf(daemon_args[i].namespace, sizeof(daemon_args[i].namespace), 
                 "multicore_ch%d", i);
        daemon_args[i].channel_id = i;
        daemon_args[i].buffer_size = buffer_size;
        daemon_args[i].running = &running[i];
        
        daemon_args[i].channel = venom_channel_create(daemon_args[i].namespace, &config);
        if (!daemon_args[i].channel) {
            fprintf(stderr, "Failed to create channel %d\n", i);
            continue;
        }
        
        pthread_create(&daemon_threads[i], NULL, daemon_thread, &daemon_args[i]);
    }
    
    usleep(50000);  /* Let daemons start */
    
    /* Start shell threads */
    printf("Starting %d shell clients...\n", num_channels);
    for (int i = 0; i < num_channels; i++) {
        strncpy(shell_args[i].namespace, daemon_args[i].namespace, 
                sizeof(shell_args[i].namespace));
        shell_args[i].channel_id = i;
        shell_args[i].message_size = message_size;
        shell_args[i].result = &results[i];
        shell_args[i].start_signal = &start_signal;
        
        pthread_create(&shell_threads[i], NULL, shell_thread, &shell_args[i]);
    }
    
    usleep(10000);  /* Let shells connect */
    
    /* Start all shells simultaneously */
    uint64_t test_start = get_time_ns();
    start_signal = 1;
    
    /* Wait for shells to complete */
    for (int i = 0; i < num_channels; i++) {
        pthread_join(shell_threads[i], NULL);
    }
    uint64_t test_end = get_time_ns();
    
    /* Stop daemons */
    for (int i = 0; i < num_channels; i++) {
        running[i] = 0;
    }
    
    /* Wake up daemons */
    for (int i = 0; i < num_channels; i++) {
        VenomChannel *ch = venom_channel_connect(daemon_args[i].namespace);
        if (ch) {
            char x = 'x';
            size_t s = 1;
            venom_request(ch, &x, 1, &x, &s, 100);
            venom_channel_disconnect(ch);
        }
    }
    
    /* Wait for daemons */
    for (int i = 0; i < num_channels; i++) {
        pthread_join(daemon_threads[i], NULL);
        venom_channel_destroy(daemon_args[i].channel);
    }
    
    /* Aggregate results */
    int total_successful = 0, total_errors = 0, total_timeouts = 0, total_data_errors = 0;
    uint64_t total_latency_ns = 0;
    uint64_t min_latency = UINT64_MAX, max_latency = 0;
    
    printf("\n┌─────────┬───────────┬────────┬──────────┬──────────────┐\n");
    printf("│ Channel │ Successful│ Errors │ Avg (µs) │ Max (µs)     │\n");
    printf("├─────────┼───────────┼────────┼──────────┼──────────────┤\n");
    
    for (int i = 0; i < num_channels; i++) {
        ChannelResult *r = &results[i];
        total_successful += r->successful;
        total_errors += r->errors;
        total_timeouts += r->timeouts;
        total_data_errors += r->data_errors;
        total_latency_ns += r->total_ns;
        
        if (r->min_ns < min_latency) min_latency = r->min_ns;
        if (r->max_ns > max_latency) max_latency = r->max_ns;
        
        double avg_us = r->successful > 0 ? (double)r->total_ns / r->successful / 1000.0 : 0;
        double max_us = (double)r->max_ns / 1000.0;
        
        printf("│   %2d    │   %5d   │   %2d   │ %8.2f │ %12.2f │\n",
               i, r->successful, r->errors + r->timeouts, avg_us, max_us);
    }
    
    printf("└─────────┴───────────┴────────┴──────────┴──────────────┘\n\n");
    
    /* Summary */
    double test_duration_s = (double)(test_end - test_start) / 1e9;
    double total_throughput = (double)total_successful / test_duration_s;
    double avg_latency_us = total_successful > 0 ? 
        (double)total_latency_ns / total_successful / 1000.0 : 0;
    double bandwidth_mbps = (double)message_size * total_successful * 2 / 
        test_duration_s / (1024.0 * 1024.0);
    
    printf("📊 AGGREGATE RESULTS:\n");
    printf("   Channels:         %d\n", num_channels);
    printf("   Total successful: %d / %d\n", total_successful, num_channels * NUM_ITERATIONS);
    printf("   Total errors:     %d\n", total_errors + total_timeouts);
    printf("   Data errors:      %d\n", total_data_errors);
    printf("   Test duration:    %.2f seconds\n", test_duration_s);
    printf("   Avg latency:      %.2f µs\n", avg_latency_us);
    printf("   Min latency:      %.2f µs\n", (double)min_latency / 1000.0);
    printf("   Max latency:      %.2f µs (%.2f ms)\n", 
           (double)max_latency / 1000.0, (double)max_latency / 1e6);
    printf("   ⚡ THROUGHPUT:     %.0f req/s (total)\n", total_throughput);
    printf("   📶 BANDWIDTH:      %.2f MB/s (total, bidirectional)\n", bandwidth_mbps);
    printf("\n");
    
    /* Cleanup */
    free(daemon_args);
    free(shell_args);
    free(results);
    free(daemon_threads);
    free(shell_threads);
    free((void*)running);
}

int main(void) {
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║         VenomMemory Multi-Core Stress Test                    ║\n");
    printf("║         Available CPUs: %-2d                                    ║\n", num_cpus);
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");
    
    /* Test with different core counts */
    int test_channels[] = {2, 4, 8, num_cpus};
    size_t test_sizes[] = {1024, 64 * 1024, 256 * 1024};  /* 1KB, 64KB, 256KB */
    
    for (size_t s = 0; s < sizeof(test_sizes)/sizeof(test_sizes[0]); s++) {
        for (size_t c = 0; c < sizeof(test_channels)/sizeof(test_channels[0]); c++) {
            if (test_channels[c] > num_cpus * 2) continue;  /* Skip unreasonable counts */
            run_multicore_test(test_channels[c], test_sizes[s]);
        }
    }
    
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                    Test Complete!                             ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    
    return 0;
}
