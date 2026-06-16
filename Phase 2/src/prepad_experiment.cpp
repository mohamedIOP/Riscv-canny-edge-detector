// ============================================================
// prepad_experiment.cpp  —  Student B auto-vectorization investigation
// ============================================================
// QUESTION: Does removing the boundary-check from the Gaussian
// inner loop allow the compiler to auto-vectorize?
//
// APPROACH: Pre-pad the image by 2 pixels on every side with zeros.
// The padded buffer is (W+4) x (H+4).  The convolution then iterates
// over the interior with NO if-guard inside the inner loop.
//
// Build:
//   g++ -std=c++17 -O3 -ftree-vectorize -fopt-info-vec-all \
//       "Phase 2/src/prepad_experiment.cpp" \
//       "Phase 2/src/gaussian.cpp" \
//       -I "Phase 2/include" \
//       -o /tmp/prepad_exp 2>prepad_vec_report.txt
//   /tmp/prepad_exp
// ============================================================

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <ctime>
#include "../include/gaussian.hpp"

#define WIDTH   256
#define HEIGHT  256
#define ITERS   100
#define RADIUS  2
#define PAD_W   (WIDTH  + 2 * RADIUS)
#define PAD_H   (HEIGHT + 2 * RADIUS)

static const int16_t GAUSS_K[25] = {
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

static uint8_t* make_padded(const uint8_t* src, int w, int h) {
    int pw = w + 2 * RADIUS;
    int ph = h + 2 * RADIUS;
    uint8_t* pad = (uint8_t*)aligned_alloc(64, (size_t)pw * ph);
    memset(pad, 0, (size_t)pw * ph);
    for (int y = 0; y < h; y++)
        memcpy(pad + (y + RADIUS) * pw + RADIUS, src + y * w, w);
    return pad;
}

// NO boundary check inside inner loop — this is what we test
static void gaussian_prepad(const uint8_t* padded_in,
                             uint8_t*       out,
                             int w, int h)
{
    int pw = w + 2 * RADIUS;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int32_t sum = 0;
            for (int ky = 0; ky < 5; ky++)
                for (int kx = 0; kx < 5; kx++)
                    sum += (int32_t)padded_in[(y + ky) * pw + (x + kx)]
                         * (int32_t)GAUSS_K[ky * 5 + kx];
            sum /= 273;
            if (sum > 255) sum = 255;
            out[y * w + x] = (uint8_t)sum;
        }
    }
}

static int compare(const uint8_t* a, const uint8_t* b, size_t n, int tol) {
    int bad = 0;
    for (size_t i = 0; i < n; i++) {
        int diff = (int)a[i] - (int)b[i];
        if (diff < 0) diff = -diff;
        if (diff > tol) bad++;
    }
    return bad;
}

int main() {
    size_t total = (size_t)WIDTH * HEIGHT;

    uint8_t* input      = (uint8_t*)aligned_alloc(64, total);
    uint8_t* out_ref    = (uint8_t*)aligned_alloc(64, total);
    uint8_t* out_prepad = (uint8_t*)aligned_alloc(64, total);

    for (size_t i = 0; i < total; i++)
        input[i] = (uint8_t)((i * 37 + 13) & 0xFF);

    uint8_t* padded = make_padded(input, WIDTH, HEIGHT);

    // Time standard Gaussian (with boundary check)
    double start = now_ms();
    for (int i = 0; i < ITERS; i++)
        canny::gaussian_blur_5x5(input, out_ref, WIDTH, HEIGHT);
    double time_ref = (now_ms() - start) / ITERS;

    // Time pre-padded Gaussian (no boundary check)
    start = now_ms();
    for (int i = 0; i < ITERS; i++)
        gaussian_prepad(padded, out_prepad, WIDTH, HEIGHT);
    double time_prepad = (now_ms() - start) / ITERS;

    int mismatches = compare(out_ref, out_prepad, total, 0);

    printf("=== Pre-Padding Auto-Vectorization Experiment ===\n");
    printf("Image: %dx%d | Iterations: %d\n\n", WIDTH, HEIGHT, ITERS);
    printf("Standard Gaussian (with boundary check) : %.4f ms/iter\n", time_ref);
    printf("Pre-padded Gaussian (no boundary check) : %.4f ms/iter\n", time_prepad);
    printf("Speedup                                 : %.2fx\n\n", time_ref / time_prepad);
    printf("Pixel mismatches (tol=0): %d\n", mismatches);
    printf("%s\n", mismatches == 0 ? "PASS: pixel-identical to reference" : "FAIL");
    printf("\nMemory overhead: %zu bytes\n", (size_t)PAD_W * PAD_H - total);

    free(input); free(out_ref); free(out_prepad); free(padded);
    return (mismatches == 0) ? 0 : 1;
}
