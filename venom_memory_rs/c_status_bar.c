/**
 * C Status Bar - Connects to Rust system_daemon via VenomMemory C Bindings
 * 
 * This demonstrates C code reading from a Rust daemon through shared memory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "venom_memory_rs.h"

// Must match SystemStats struct in system_daemon.rs exactly!
typedef struct __attribute__((packed)) {
    float cpu_usage_percent;
    float cpu_cores[16];
    uint32_t core_count;
    uint32_t memory_used_mb;
    uint32_t memory_total_mb;
    uint64_t uptime_seconds;
    uint64_t timestamp_ns;
} SystemStats;

void print_bar(float percent, int width) {
    int filled = (int)((percent / 100.0f) * width);
    printf("[");
    for (int i = 0; i < width; i++) {
        if (i < filled) {
            if (percent > 80) printf("\033[91m█\033[0m");      // Red
            else if (percent > 50) printf("\033[93m▓\033[0m"); // Yellow
            else printf("\033[92m░\033[0m");                    // Green
        } else {
            printf(" ");
        }
    }
    printf("]");
}

void clear_screen() {
    printf("\033[2J\033[H");
}

int main() {
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║   VenomMemory C Status Bar                                    ║\n");
    printf("║   Connecting to Rust system_daemon via C Bindings             ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n\n");

    // Connect to the Rust daemon's channel
    VenomShellHandle* shell = venom_shell_connect("system_monitor");
    if (!shell) {
        fprintf(stderr, "❌ Failed to connect to system_monitor channel!\n");
        fprintf(stderr, "   Make sure system_daemon is running:\n");
        fprintf(stderr, "   cargo run --release --example system_daemon\n");
        return 1;
    }

    printf("✅ Connected! Shell ID: %u\n", venom_shell_id(shell));
    printf("📊 Reading system stats from Rust daemon...\n\n");
    sleep(1);

    uint8_t* buf = malloc(sizeof(SystemStats) + 128);
    int frame = 0;

    while (1) {
        // Read from shared memory (this is the C binding call!)
        size_t len = venom_shell_read_data(shell, buf, sizeof(SystemStats) + 128);

        if (len >= sizeof(SystemStats)) {
            SystemStats* stats = (SystemStats*)buf;
            
            clear_screen();
            
            printf("╔═══════════════════════════════════════════════════════════════╗\n");
            printf("║  🖥️  VenomMemory C Monitor          Frame: %-6d             ║\n", frame++);
            printf("║      (Reading from Rust Daemon via C Bindings)                ║\n");
            printf("╠═══════════════════════════════════════════════════════════════╣\n");
            
            // CPU Total
            printf("║  CPU Total: ");
            print_bar(stats->cpu_usage_percent, 25);
            printf(" %5.1f%%           ║\n", stats->cpu_usage_percent);
            
            printf("╠═══════════════════════════════════════════════════════════════╣\n");
            
            // Per-core (show up to 8)
            int cores = stats->core_count > 8 ? 8 : stats->core_count;
            for (int i = 0; i < cores; i++) {
                printf("║    Core %d: ", i);
                print_bar(stats->cpu_cores[i], 20);
                printf(" %5.1f%%                ║\n", stats->cpu_cores[i]);
            }
            
            printf("╠═══════════════════════════════════════════════════════════════╣\n");
            
            // Memory
            float mem_pct = (float)stats->memory_used_mb / stats->memory_total_mb * 100.0f;
            printf("║  RAM: ");
            print_bar(mem_pct, 25);
            printf(" %u/%u MB         ║\n", 
                stats->memory_used_mb, stats->memory_total_mb);
            
            printf("╠═══════════════════════════════════════════════════════════════╣\n");
            
            // Uptime
            uint64_t s = stats->uptime_seconds;
            uint64_t h = s / 3600;
            uint64_t m = (s % 3600) / 60;
            printf("║  ⏱️  Uptime: %luh %lum                                         ║\n", 
                (unsigned long)h, (unsigned long)m);
            
            printf("╚═══════════════════════════════════════════════════════════════╝\n");
            printf("\n  Press Ctrl+C to exit\n");
        } else {
            printf("⏳ Waiting for data from daemon... (got %zu bytes)\n", len);
        }

        usleep(100000); // 100ms
    }

    free(buf);
    venom_shell_destroy(shell);
    return 0;
}
