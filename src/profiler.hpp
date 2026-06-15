#pragma once
#include <cstdint>
#include <cstdio>

// ============================================================
// Profiler — Per-stage timing harness
// ============================================================
// Uses CLOCK_MONOTONIC for wall-clock timing.
// QEMU is not cycle-accurate, so absolute numbers are
// meaningless. Only relative comparisons are valid
// (e.g. O0 vs O3, scalar vs RVV).
// ============================================================

// Bare-metal RISC-V syscall for clock_gettime
// syscall number 113 = SYS_clock_gettime on Linux RISC-V
struct timespec_rv {
    long tv_sec;
    long tv_nsec;
};

static inline uint64_t now_ns() {
    struct timespec_rv ts;
    // CLOCK_MONOTONIC = 1
    register long a0 asm("a0") = 1;          // clockid = CLOCK_MONOTONIC
    register long a1 asm("a1") = (long)&ts;  // pointer to timespec
    register long a7 asm("a7") = 113;        // SYS_clock_gettime
    asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a7) : "memory");
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// Convert nanoseconds to milliseconds
static inline double ns_to_ms(uint64_t ns) {
    return (double)ns / 1000000.0;
}

// ============================================================
// StageTiming — holds results for one pipeline stage
// ============================================================
struct StageTiming {
    const char* name;
    double      total_ms;
    double      avg_ms;
    double      pct;
};

// ============================================================
// print_timing_table
// ============================================================
static inline void print_timing_table(StageTiming* stages,
                                       int n_stages,
                                       int iterations,
                                       size_t binary_size_kb) {
    double total = 0.0;
    for (int i = 0; i < n_stages; i++)
        total += stages[i].avg_ms;

    for (int i = 0; i < n_stages; i++)
        stages[i].pct = (total > 0) ? (stages[i].avg_ms / total) * 100.0 : 0.0;

    printf("\n=== Pipeline Timing Report ===\n");
    printf("Iterations : %d\n", iterations);
    printf("Binary size: %zu KB\n", binary_size_kb);
    printf("----------------------------------------------\n");
    printf("%-20s %10s %8s\n", "Stage", "Avg (ms)", "% Time");
    printf("----------------------------------------------\n");
    for (int i = 0; i < n_stages; i++) {
        printf("%-20s %10.4f %7.1f%%\n",
               stages[i].name,
               stages[i].avg_ms,
               stages[i].pct);
    }
    printf("----------------------------------------------\n");
    printf("%-20s %10.4f %7.1f%%\n", "TOTAL", total, 100.0);
    printf("==============================================\n\n");
}