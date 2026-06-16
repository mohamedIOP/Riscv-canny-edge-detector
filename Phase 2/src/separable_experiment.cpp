#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <ctime>
#include "../include/gaussian.hpp"

#define WIDTH  256
#define HEIGHT 256
#define ITERS  100

static double now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main() {
    size_t total = WIDTH * HEIGHT;

    uint8_t* input  = (uint8_t*)aligned_alloc(64, total);
    uint8_t* out_2d = (uint8_t*)aligned_alloc(64, total);
    uint8_t* out_sp = (uint8_t*)aligned_alloc(64, total);

    // Fill input with random data
    for (size_t i = 0; i < total; i++)
        input[i] = (uint8_t)(i % 256);

    // ── Time 2D 5x5 ──────────────────────────────────────────────────────
    double start = now_ms();
    for (int i = 0; i < ITERS; i++)
        canny::gaussian_blur_5x5(input, out_2d, WIDTH, HEIGHT);
    double time_2d = (now_ms() - start) / ITERS;

    // ── Time Separable 1x5 + 5x1 ─────────────────────────────────────────
    start = now_ms();
    for (int i = 0; i < ITERS; i++)
        canny::gaussian_blur_5x5_separable(input, out_sp, WIDTH, HEIGHT);
    double time_sep = (now_ms() - start) / ITERS;

    // ── Verify outputs are close (allow +-2 per pixel) ───────────────────
    int max_diff = 0;
    int diff_count = 0;
    for (size_t i = 0; i < total; i++) {
        int diff = (int)out_2d[i] - (int)out_sp[i];
        if (diff < 0) diff = -diff;
        if (diff > max_diff) max_diff = diff;
        if (diff > 2) diff_count++;
    }

    // ── Print Results ─────────────────────────────────────────────────────
    printf("=== Separable Filter Experiment ===\n");
    printf("Image size : %dx%d\n", WIDTH, HEIGHT);
    printf("Iterations : %d\n\n", ITERS);
    printf("2D 5x5      : %.4f ms/iter\n", time_2d);
    printf("Separable   : %.4f ms/iter\n", time_sep);
    printf("Speedup     : %.2fx\n\n", time_2d / time_sep);
    printf("Max pixel diff : %d\n", max_diff);
    printf("Pixels > 2 diff: %d / %zu\n", diff_count, total);

    free(input);
    free(out_2d);
    free(out_sp);
    return 0;
}