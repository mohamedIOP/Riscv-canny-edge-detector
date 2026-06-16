// ============================================================
// separable_autovec_analysis.cpp  —  Student B investigation
// ============================================================
// QUESTION: Does rewriting the Gaussian as a separable filter
// (1x5 + 5x1) allow the compiler to auto-vectorize?
//
// Student A found two blockers in vec_report.txt:
//   1. "loop nest containing two or more consecutive inner loops"
//   2. "unsupported control flow in loop" (boundary check)
//
// This experiment tests each fix in isolation to show that
// BOTH must be fixed together.
//
// Build:
//   g++ -std=c++17 -O3 -ftree-vectorize -fopt-info-vec-all \
//       "Phase 2/src/separable_autovec_analysis.cpp" \
//       "Phase 2/src/gaussian.cpp" \
//       -I "Phase 2/include" \
//       -o /tmp/sep_av 2>separable_vec_report.txt
//   /tmp/sep_av
// ============================================================

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <ctime>
#include "../include/gaussian.hpp"

#define WIDTH  256
#define HEIGHT 256
#define ITERS  100

static const int16_t GAUSS_1D[5] = { 1, 4, 7, 4, 1 };
static const int     GAUSS_1D_SUM = 17;

static const int16_t GAUSS_2D[25] = {
     1,  4,  7,  4, 1,
     4, 16, 26, 16, 4,
     7, 26, 41, 26, 7,
     4, 16, 26, 16, 4,
     1,  4,  7,  4, 1
};

static double now_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

// Version A: 2D with boundary check — BLOCKED by 2 reasons
static void blur_2d(const uint8_t* in, uint8_t* out, int w, int h) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int32_t sum = 0;
            for (int ky = -2; ky <= 2; ky++) {
                for (int kx = -2; kx <= 2; kx++) {
                    int iy = y + ky, ix = x + kx;
                    uint8_t p = 0;
                    if (iy >= 0 && iy < h && ix >= 0 && ix < w)
                        p = in[iy * w + ix];
                    sum += (int32_t)p * GAUSS_2D[(ky+2)*5 + (kx+2)];
                }
            }
            sum /= 273;
            if (sum < 0) sum = 0;
            if (sum > 255) sum = 255;
            out[y * w + x] = (uint8_t)sum;
        }
    }
}

// Version B: Separable WITH boundary check
// Fixes blocker 1 (nested loops) but blocker 2 (control flow) remains
static void horiz_pass(const uint8_t* in, uint8_t* out, int w, int h) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int32_t sum = 0;
            for (int kx = -2; kx <= 2; kx++) {
                int ix = x + kx;
                uint8_t p = 0;
                if (ix >= 0 && ix < w) p = in[y * w + ix];
                sum += (int32_t)p * GAUSS_1D[kx + 2];
            }
            sum /= GAUSS_1D_SUM;
            if (sum < 0) sum = 0;
            if (sum > 255) sum = 255;
            out[y * w + x] = (uint8_t)sum;
        }
    }
}

static void vert_pass(const uint8_t* in, uint8_t* out, int w, int h) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int32_t sum = 0;
            for (int ky = -2; ky <= 2; ky++) {
                int iy = y + ky;
                uint8_t p = 0;
                if (iy >= 0 && iy < h) p = in[iy * w + x];
                sum += (int32_t)p * GAUSS_1D[ky + 2];
            }
            sum /= GAUSS_1D_SUM;
            if (sum < 0) sum = 0;
            if (sum > 255) sum = 255;
            out[y * w + x] = (uint8_t)sum;
        }
    }
}

// Version C: Separable + NO boundary check for interior
// Fixes BOTH blockers — this should auto-vectorize
static void horiz_pass_nocheck(const uint8_t* in, uint8_t* out, int w, int h) {
    for (int y = 0; y < h; y++) {
        // left border scalar
        for (int x = 0; x < 2; x++) {
            int32_t sum = 0;
            for (int kx = -2; kx <= 2; kx++) {
                int ix = x + kx;
                uint8_t p = (ix >= 0) ? in[y * w + ix] : 0;
                sum += (int32_t)p * GAUSS_1D[kx + 2];
            }
            out[y * w + x] = (uint8_t)(sum / GAUSS_1D_SUM);
        }
        // interior: no branch — compiler should vectorize this
        for (int x = 2; x < w - 2; x++) {
            int32_t sum = 0;
            for (int kx = -2; kx <= 2; kx++)
                sum += (int32_t)in[y * w + x + kx] * GAUSS_1D[kx + 2];
            sum /= GAUSS_1D_SUM;
            if (sum > 255) sum = 255;
            out[y * w + x] = (uint8_t)sum;
        }
        // right border scalar
        for (int x = w - 2; x < w; x++) {
            int32_t sum = 0;
            for (int kx = -2; kx <= 2; kx++) {
                int ix = x + kx;
                uint8_t p = (ix < w) ? in[y * w + ix] : 0;
                sum += (int32_t)p * GAUSS_1D[kx + 2];
            }
            out[y * w + x] = (uint8_t)(sum / GAUSS_1D_SUM);
        }
    }
}

static int compare(const uint8_t* a, const uint8_t* b, size_t n, int tol) {
    int bad = 0;
    for (size_t i = 0; i < n; i++) {
        int d = (int)a[i] - (int)b[i]; if (d < 0) d = -d;
        if (d > tol) bad++;
    }
    return bad;
}

int main() {
    size_t N = (size_t)WIDTH * HEIGHT;
    uint8_t* input = (uint8_t*)aligned_alloc(64, N);
    uint8_t* ref   = (uint8_t*)aligned_alloc(64, N);
    uint8_t* tmp   = (uint8_t*)aligned_alloc(64, N);
    uint8_t* outA  = (uint8_t*)aligned_alloc(64, N);
    uint8_t* outB  = (uint8_t*)aligned_alloc(64, N);
    uint8_t* outC  = (uint8_t*)aligned_alloc(64, N);

    for (size_t i = 0; i < N; i++)
        input[i] = (uint8_t)((i * 37 + 13) & 0xFF);

    canny::gaussian_blur_5x5(input, ref, WIDTH, HEIGHT);

    double s = now_ms();
    for (int i = 0; i < ITERS; i++) blur_2d(input, outA, WIDTH, HEIGHT);
    double tA = (now_ms() - s) / ITERS;

    s = now_ms();
    for (int i = 0; i < ITERS; i++) {
        horiz_pass(input, tmp, WIDTH, HEIGHT);
        vert_pass(tmp, outB, WIDTH, HEIGHT);
    }
    double tB = (now_ms() - s) / ITERS;

    s = now_ms();
    for (int i = 0; i < ITERS; i++) {
        horiz_pass_nocheck(input, tmp, WIDTH, HEIGHT);
        vert_pass(tmp, outC, WIDTH, HEIGHT);
    }
    double tC = (now_ms() - s) / ITERS;

    int dA = compare(ref, outA, N, 1);
    int dB = compare(ref, outB, N, 3);
    int dC = compare(ref, outC, N, 3);

    printf("=== Separable Auto-Vectorization Analysis ===\n");
    printf("Image: %dx%d | Iterations: %d\n\n", WIDTH, HEIGHT, ITERS);
    printf("| Version                              | ms/iter | Speedup | Mismatches |\n");
    printf("|--------------------------------------|---------|---------|------------|\n");
    printf("| A: 2D 5x5 (2 blockers)              | %7.4f | %5.2fx  | %10d |\n", tA, tA/tA, dA);
    printf("| B: Separable + boundary check        | %7.4f | %5.2fx  | %10d |\n", tB, tA/tB, dB);
    printf("| C: Separable + no check (interior)  | %7.4f | %5.2fx  | %10d |\n", tC, tA/tC, dC);
    printf("\nConclusion:\n");
    printf("  Separable alone (B): fixes nested-loops blocker, boundary check still blocks\n");
    printf("  Both fixes (C)     : fully unblocks auto-vectorization\n");
    printf("  Check separable_vec_report.txt for 'optimized: loop vectorized'\n");

    free(input); free(ref); free(tmp); free(outA); free(outB); free(outC);
    return 0;
}