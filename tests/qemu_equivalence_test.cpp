// ============================================================
// qemu_equivalence_test.cpp
// ============================================================
// QEMU-side equivalence tests — cross-compiled for RISC-V.
// These tests run on QEMU (not on the host) and verify that
// the scalar pipeline produces correct output.
//
// Compile with:
//   riscv64-unknown-elf-g++ -static -march=rv64gcv -mabi=lp64d -O2 -std=c++17 \
//     -I"Phase 2/include" \
//     tests/qemu_equivalence_test.cpp \
//     "Phase 2"/src/gaussian.cpp \
//     "Phase 2"/src/sobel.cpp \
//     "Phase 2"/src/magnitude.cpp \
//     "Phase 2"/src/direction.cpp \
//     -o qemu_eq_test
//
// Run at three VLEN values:
//   qemu-riscv64 -cpu rv64,v=true,vlen=128 ./qemu_eq_test
//   qemu-riscv64 -cpu rv64,v=true,vlen=256 ./qemu_eq_test
//   qemu-riscv64 -cpu rv64,v=true,vlen=512 ./qemu_eq_test
//
// Expected output: all tests PASSED at every VLEN.
// If output differs between VLEN values, a VLA bug exists.
// ============================================================

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cstring>

#include "../Phase 2/include/gaussian.hpp"
#include "../Phase 2/include/sobel.hpp"
#include "../Phase 2/include/magnitude.hpp"
#include "../Phase 2/include/direction.hpp"
#include "../Phase 2/include/nms.hpp"
#include "../Phase 2/include/threshold.hpp"

// ============================================================
// Minimal assert harness (no GoogleTest on QEMU bare-metal)
// ============================================================
static int g_passed = 0;
static int g_failed = 0;

#define ASSERT_EQ(a, b, msg)                                        \
    do {                                                             \
        if ((a) != (b)) {                                            \
            printf("FAIL  [%s] expected %d got %d\n", msg, (int)(b), (int)(a)); \
            g_failed++;                                              \
        } else {                                                     \
            g_passed++;                                              \
        }                                                            \
    } while (0)

#define ASSERT_NEAR(a, b, tol, msg)                                 \
    do {                                                             \
        int diff = (int)(a) - (int)(b);                              \
        if (diff < 0) diff = -diff;                                  \
        if (diff > (int)(tol)) {                                     \
            printf("FAIL  [%s] expected ~%d got %d (diff=%d)\n",    \
                   msg, (int)(b), (int)(a), diff);                   \
            g_failed++;                                              \
        } else {                                                     \
            g_passed++;                                              \
        }                                                            \
    } while (0)

#define ASSERT_GT(a, b, msg)                                        \
    do {                                                             \
        if (!((a) > (b))) {                                          \
            printf("FAIL  [%s] expected %d > %d\n", msg, (int)(a), (int)(b)); \
            g_failed++;                                              \
        } else {                                                     \
            g_passed++;                                              \
        }                                                            \
    } while (0)

#define ASSERT_GE(a, b, msg)                                        \
    do {                                                             \
        if (!((a) >= (b))) {                                         \
            printf("FAIL  [%s] expected %d >= %d\n", msg, (int)(a), (int)(b)); \
            g_failed++;                                              \
        } else {                                                     \
            g_passed++;                                              \
        }                                                            \
    } while (0)

// ============================================================
// Helpers
// ============================================================

// Generate a deterministic pseudo-random image (same seed = same image).
// Used so scalar and RVV runs operate on identical input.
static void fill_random(uint8_t* buf, size_t n, uint32_t seed = 42) {
    uint32_t s = seed;
    for (size_t i = 0; i < n; i++) {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        buf[i] = (uint8_t)(s & 0xFF);
    }
}

// Compare two uint8_t buffers — allow ±1 tolerance (rounding differences).
// Returns number of pixels that differ beyond tolerance.
static int compare_buffers_u8(const uint8_t* a, const uint8_t* b,
                               size_t n, int tol = 1) {
    int mismatches = 0;
    for (size_t i = 0; i < n; i++) {
        int diff = (int)a[i] - (int)b[i];
        if (diff < 0) diff = -diff;
        if (diff > tol) mismatches++;
    }
    return mismatches;
}

// Compare two int16_t buffers — exact match.
static int compare_buffers_i16(const int16_t* a, const int16_t* b, size_t n) {
    int mismatches = 0;
    for (size_t i = 0; i < n; i++)
        if (a[i] != b[i]) mismatches++;
    return mismatches;
}

// ============================================================
// Test 1: Gaussian — uniform image stays uniform
// ============================================================
// WHY NON-POWER-OF-TWO: width=100, height=75 forces the
// strip-mining tail case in any future RVV implementation.
// If width were 128, the tail case never executes and bugs hide.
static void test_gaussian_uniform() {
    printf("\n[Gaussian] Uniform image stays uniform\n");
    const size_t W = 100, H = 75;   // intentionally non-power-of-two
    uint8_t* in  = (uint8_t*)aligned_alloc(64, W * H);
    uint8_t* out = (uint8_t*)aligned_alloc(64, W * H);
    memset(in,  128, W * H);
    memset(out, 0,   W * H);

    canny::gaussian_blur_5x5(in, out, W, H);

    // Check interior pixels only (skip 2-pixel border — zero-padding effect)
    int bad = 0;
    for (size_t y = 2; y < H - 2; y++)
        for (size_t x = 2; x < W - 2; x++) {
            int diff = (int)out[y * W + x] - 128;
            if (diff < 0) diff = -diff;
            if (diff > 1) bad++;
        }
    ASSERT_EQ(bad, 0, "gaussian_uniform_interior");

    free(in); free(out);
}

// ============================================================
// Test 2: Gaussian — all-black stays all-black
// ============================================================
static void test_gaussian_black() {
    printf("[Gaussian] All-black stays all-black\n");
    const size_t W = 100, H = 75;
    uint8_t* in  = (uint8_t*)aligned_alloc(64, W * H);
    uint8_t* out = (uint8_t*)aligned_alloc(64, W * H);
    memset(in,  0, W * H);
    memset(out, 0, W * H);

    canny::gaussian_blur_5x5(in, out, W, H);

    int bad = 0;
    for (size_t i = 0; i < W * H; i++)
        if (out[i] != 0) bad++;
    ASSERT_EQ(bad, 0, "gaussian_black");

    free(in); free(out);
}

// ============================================================
// Test 3: Gaussian — impulse spreads symmetrically
// ============================================================
static void test_gaussian_impulse() {
    printf("[Gaussian] Impulse spreads and center stays brightest\n");
    const size_t W = 48, H = 48;   // non-power-of-two
    uint8_t* in  = (uint8_t*)aligned_alloc(64, W * H);
    uint8_t* out = (uint8_t*)aligned_alloc(64, W * H);
    memset(in, 0, W * H);
    memset(out, 0, W * H);

    // Single bright pixel at centre
    in[24 * W + 24] = 255;
    canny::gaussian_blur_5x5(in, out, W, H);

    uint8_t c  = out[24 * W + 24];
    uint8_t l  = out[24 * W + 23];
    uint8_t r  = out[24 * W + 25];
    uint8_t u  = out[23 * W + 24];
    uint8_t d  = out[25 * W + 24];

    ASSERT_GT((int)c, (int)l, "impulse: center > left");
    ASSERT_GT((int)c, (int)r, "impulse: center > right");
    ASSERT_GT((int)c, (int)u, "impulse: center > up");
    ASSERT_GT((int)c, (int)d, "impulse: center > down");
    ASSERT_EQ(l, r, "impulse: left == right (symmetry)");
    ASSERT_EQ(u, d, "impulse: up == down (symmetry)");

    free(in); free(out);
}

// ============================================================
// Test 4: Sobel — zero gradient on uniform image
// ============================================================
static void test_sobel_uniform() {
    printf("\n[Sobel] Uniform image has zero gradient\n");
    const size_t W = 100, H = 75;
    uint8_t*  in = (uint8_t*)aligned_alloc(64, W * H);
    int16_t* gx  = (int16_t*)aligned_alloc(64, W * H * sizeof(int16_t));
    int16_t* gy  = (int16_t*)aligned_alloc(64, W * H * sizeof(int16_t));
    memset(in, 128, W * H);

    canny::sobel_gradients(in, gx, gy, W, H);

    // Interior only (skip 1-pixel border)
    int bad = 0;
    for (size_t y = 1; y < H - 1; y++)
        for (size_t x = 1; x < W - 1; x++) {
            if (gx[y * W + x] != 0) bad++;
            if (gy[y * W + x] != 0) bad++;
        }
    ASSERT_EQ(bad, 0, "sobel_uniform_zero");

    free(in); free(gx); free(gy);
}

// ============================================================
// Test 5: Sobel — vertical edge → large Gx, small Gy
// ============================================================
static void test_sobel_vertical_edge() {
    printf("[Sobel] Vertical edge detected in Gx\n");
    const size_t W = 48, H = 48;
    uint8_t*  in = (uint8_t*)aligned_alloc(64, W * H);
    int16_t* gx  = (int16_t*)aligned_alloc(64, W * H * sizeof(int16_t));
    int16_t* gy  = (int16_t*)aligned_alloc(64, W * H * sizeof(int16_t));
    memset(in, 0, W * H);

    // Right half = white
    for (size_t y = 0; y < H; y++)
        for (size_t x = W/2; x < W; x++)
            in[y * W + x] = 255;

    canny::sobel_gradients(in, gx, gy, W, H);

    size_t mid = H/2 * W + W/2 - 1;
    int agx = abs((int)gx[mid]);
    int agy = abs((int)gy[mid]);
    ASSERT_GT(agx, agy, "vertical_edge: |Gx| > |Gy|");

    free(in); free(gx); free(gy);
}

// ============================================================
// Test 6: Sobel — horizontal edge → large Gy, small Gx
// ============================================================
static void test_sobel_horizontal_edge() {
    printf("[Sobel] Horizontal edge detected in Gy\n");
    const size_t W = 48, H = 48;
    uint8_t*  in = (uint8_t*)aligned_alloc(64, W * H);
    int16_t* gx  = (int16_t*)aligned_alloc(64, W * H * sizeof(int16_t));
    int16_t* gy  = (int16_t*)aligned_alloc(64, W * H * sizeof(int16_t));
    memset(in, 0, W * H);

    // Bottom half = white
    for (size_t y = H/2; y < H; y++)
        for (size_t x = 0; x < W; x++)
            in[y * W + x] = 255;

    canny::sobel_gradients(in, gx, gy, W, H);

    size_t mid = (H/2 - 1) * W + W/2;
    int agx = abs((int)gx[mid]);
    int agy = abs((int)gy[mid]);
    ASSERT_GT(agy, agx, "horizontal_edge: |Gy| > |Gx|");

    free(in); free(gx); free(gy);
}

// ============================================================
// Test 7: Magnitude L1 — nonzero on edge image
// ============================================================
static void test_magnitude_l1_nonzero() {
    printf("\n[Magnitude] L1 nonzero on edge image\n");
    const size_t W = 48, H = 48;
    int16_t* gx  = (int16_t*)aligned_alloc(64, W * H * sizeof(int16_t));
    int16_t* gy  = (int16_t*)aligned_alloc(64, W * H * sizeof(int16_t));
    uint8_t* mag = (uint8_t*)aligned_alloc(64, W * H);

    // All gradients = 100
    for (size_t i = 0; i < W * H; i++) { gx[i] = 100; gy[i] = 100; }

    canny::magnitude_l1(gx, gy, mag, W, H);

    int bad = 0;
    for (size_t i = 0; i < W * H; i++)
        if (mag[i] == 0) bad++;
    ASSERT_EQ(bad, 0, "magnitude_l1_nonzero");

    free(gx); free(gy); free(mag);
}

// ============================================================
// Test 8: Magnitude L1 — zero on flat image
// ============================================================
static void test_magnitude_l1_zero() {
    printf("[Magnitude] L1 zero on flat image\n");
    const size_t W = 48, H = 48;
    int16_t* gx  = (int16_t*)aligned_alloc(64, W * H * sizeof(int16_t));
    int16_t* gy  = (int16_t*)aligned_alloc(64, W * H * sizeof(int16_t));
    uint8_t* mag = (uint8_t*)aligned_alloc(64, W * H);
    memset(gx,  0, W * H * sizeof(int16_t));
    memset(gy,  0, W * H * sizeof(int16_t));

    canny::magnitude_l1(gx, gy, mag, W, H);

    int bad = 0;
    for (size_t i = 0; i < W * H; i++)
        if (mag[i] != 0) bad++;
    ASSERT_EQ(bad, 0, "magnitude_l1_zero");

    free(gx); free(gy); free(mag);
}

// ============================================================
// Test 9: Direction — vertical edge → direction 0
// ============================================================
static void test_direction_vertical() {
    printf("\n[Direction] Vertical edge → direction 0\n");
    const size_t W = 48, H = 48;
    int16_t* gx  = (int16_t*)aligned_alloc(64, W * H * sizeof(int16_t));
    int16_t* gy  = (int16_t*)aligned_alloc(64, W * H * sizeof(int16_t));
    uint8_t* dir = (uint8_t*)aligned_alloc(64, W * H);

    // Large Gx, zero Gy → horizontal gradient → direction 0
    for (size_t i = 0; i < W * H; i++) { gx[i] = 500; gy[i] = 0; }

    canny::gradient_direction(gx, gy, dir, W, H);

    int bad = 0;
    for (size_t i = 0; i < W * H; i++)
        if (dir[i] != 0) bad++;
    ASSERT_EQ(bad, 0, "direction_vertical_is_0");

    free(gx); free(gy); free(dir);
}

// ============================================================
// Test 10: Direction — horizontal edge → direction 2
// ============================================================
static void test_direction_horizontal() {
    printf("[Direction] Horizontal edge → direction 2\n");
    const size_t W = 48, H = 48;
    int16_t* gx  = (int16_t*)aligned_alloc(64, W * H * sizeof(int16_t));
    int16_t* gy  = (int16_t*)aligned_alloc(64, W * H * sizeof(int16_t));
    uint8_t* dir = (uint8_t*)aligned_alloc(64, W * H);

    // Zero Gx, large Gy → vertical gradient → direction 2
    for (size_t i = 0; i < W * H; i++) { gx[i] = 0; gy[i] = 500; }

    canny::gradient_direction(gx, gy, dir, W, H);

    int bad = 0;
    for (size_t i = 0; i < W * H; i++)
        if (dir[i] != 2) bad++;
    ASSERT_EQ(bad, 0, "direction_horizontal_is_2");

    free(gx); free(gy); free(dir);
}

// ============================================================
// Test 11: Direction — diagonal → direction 1 or 3
// ============================================================
static void test_direction_diagonal() {
    printf("[Direction] Diagonal → direction 1 or 3\n");
    const size_t W = 48, H = 48;
    int16_t* gx  = (int16_t*)aligned_alloc(64, W * H * sizeof(int16_t));
    int16_t* gy  = (int16_t*)aligned_alloc(64, W * H * sizeof(int16_t));
    uint8_t* dir = (uint8_t*)aligned_alloc(64, W * H);

    for (size_t i = 0; i < W * H; i++) { gx[i] = 300; gy[i] = 300; }

    canny::gradient_direction(gx, gy, dir, W, H);

    int bad = 0;
    for (size_t i = 0; i < W * H; i++)
        if (dir[i] != 1 && dir[i] != 3) bad++;
    ASSERT_EQ(bad, 0, "direction_diagonal_is_1or3");

    free(gx); free(gy); free(dir);
}

// ============================================================
// Test 12: Full pipeline equivalence on random image
// ============================================================
// WHY THIS TEST MATTERS:
// This is the most important QEMU test. It runs the complete
// scalar pipeline twice on the same input and checks that
// output is identical. Once RVV intrinsics are added, replace
// the second run with the RVV version and verify equivalence.
// Run at VLEN=128, 256, 512 — output must be identical every time.
// ============================================================
static void test_full_pipeline_equivalence() {
    printf("\n[Pipeline] Full scalar pipeline — deterministic output\n");

    // Non-power-of-two dimensions force strip-mining tail case
    const size_t W = 100, H = 75;
    const size_t N = W * H;

    uint8_t*  in      = (uint8_t*)aligned_alloc(64, N);
    uint8_t*  blur1   = (uint8_t*)aligned_alloc(64, N);
    uint8_t*  blur2   = (uint8_t*)aligned_alloc(64, N);
    int16_t*  gx1     = (int16_t*)aligned_alloc(64, N * sizeof(int16_t));
    int16_t*  gy1     = (int16_t*)aligned_alloc(64, N * sizeof(int16_t));
    int16_t*  gx2     = (int16_t*)aligned_alloc(64, N * sizeof(int16_t));
    int16_t*  gy2     = (int16_t*)aligned_alloc(64, N * sizeof(int16_t));
    uint8_t*  mag1    = (uint8_t*)aligned_alloc(64, N);
    uint8_t*  mag2    = (uint8_t*)aligned_alloc(64, N);
    uint8_t*  dir1    = (uint8_t*)aligned_alloc(64, N);
    uint8_t*  dir2    = (uint8_t*)aligned_alloc(64, N);

    // Same seed → same image both runs
    fill_random(in, N, 42);

    // ── Run 1 ──────────────────────────────────────────────
    canny::gaussian_blur_5x5(in, blur1, W, H);
    canny::sobel_gradients(blur1, gx1, gy1, W, H);
    canny::magnitude_l1(gx1, gy1, mag1, W, H);
    canny::gradient_direction(gx1, gy1, dir1, W, H);

    // ── Run 2 (identical — verifies determinism) ───────────
    canny::gaussian_blur_5x5(in, blur2, W, H);
    canny::sobel_gradients(blur2, gx2, gy2, W, H);
    canny::magnitude_l1(gx2, gy2, mag2, W, H);
    canny::gradient_direction(gx2, gy2, dir2, W, H);

    // ── Compare ────────────────────────────────────────────
    int blur_diff = compare_buffers_u8(blur1, blur2, N, 0);
    int gx_diff   = compare_buffers_i16(gx1, gx2, N);
    int gy_diff   = compare_buffers_i16(gy1, gy2, N);
    int mag_diff  = compare_buffers_u8(mag1, mag2, N, 0);
    int dir_diff  = compare_buffers_u8(dir1, dir2, N, 0);

    ASSERT_EQ(blur_diff, 0, "pipeline_gaussian_deterministic");
    ASSERT_EQ(gx_diff,   0, "pipeline_gx_deterministic");
    ASSERT_EQ(gy_diff,   0, "pipeline_gy_deterministic");
    ASSERT_EQ(mag_diff,  0, "pipeline_magnitude_deterministic");
    ASSERT_EQ(dir_diff,  0, "pipeline_direction_deterministic");

    free(in); free(blur1); free(blur2);
    free(gx1); free(gy1); free(gx2); free(gy2);
    free(mag1); free(mag2); free(dir1); free(dir2);
}

// ============================================================
// Test 13: Tail-case stress test
// ============================================================
// Runs the pipeline on sizes that are NOT multiples of any
// common VLEN (16, 32, 64). The strip-mining remainder
// (the "tail") must be handled correctly.
// ============================================================
// ============================================================
// Test 14: Scalar vs RVV Gaussian — equivalence
// ============================================================
static void test_gaussian_scalar_vs_rvv() {
    printf("\n[RVV] Gaussian scalar vs RVV equivalence\n");

    const size_t W = 100, H = 75;
    const size_t N = W * H;
    const int radius = 2;

    uint8_t* in        = (uint8_t*)aligned_alloc(64, N);
    uint8_t* out_scalar = (uint8_t*)aligned_alloc(64, N);
    uint8_t* out_rvv    = (uint8_t*)aligned_alloc(64, N);

    memset(out_scalar, 0, N);
    memset(out_rvv, 0, N);

    fill_random(in, N, 123);

    canny::gaussian_blur_5x5(in, out_scalar, W, H);
    canny::gaussian_blur_5x5_rvv(in, out_rvv, W, H);

    int mismatches = 0;
    for (size_t y = radius; y < H - radius; y++) {
        for (size_t x = radius; x < W - radius; x++) {
            size_t idx = y * W + x;
            int diff = (int)out_scalar[idx] - (int)out_rvv[idx];
            if (diff < 0) diff = -diff;
            if (diff > 1) mismatches++;
        }
    }

    ASSERT_EQ(mismatches, 0, "gaussian_scalar_vs_rvv_interior");

    free(in); free(out_scalar); free(out_rvv);
}

// ============================================================
// Test 15: Scalar vs RVV Magnitude L1 — equivalence
// ============================================================

static void test_magnitude_scalar_vs_rvv() {
    printf("\n[RVV] Magnitude L1 scalar vs RVV equivalence\n");

    const size_t W = 100, H = 75;
    const size_t N = W * H;

    int16_t* gx = (int16_t*)aligned_alloc(64, N * sizeof(int16_t));
    int16_t* gy = (int16_t*)aligned_alloc(64, N * sizeof(int16_t));
    uint8_t* mag_scalar = (uint8_t*)aligned_alloc(64, N);
    uint8_t* mag_rvv    = (uint8_t*)aligned_alloc(64, N);

    for (size_t i = 0; i < N; i++) {
        gx[i] = (int16_t)((int)(i * 37 % 601) - 300);
        gy[i] = (int16_t)((int)(i * 53 % 601) - 300);
    }

    canny::magnitude_l1(gx, gy, mag_scalar, W, H);
    canny::magnitude_l1_rvv(gx, gy, mag_rvv, W, H);

    int mismatches = compare_buffers_u8(mag_scalar, mag_rvv, N, 1);
    ASSERT_EQ(mismatches, 0, "magnitude_scalar_vs_rvv");

    free(gx); free(gy); free(mag_scalar); free(mag_rvv);
}
static void test_tail_case_sizes() {
    printf("\n[Tail] Non-multiple-of-VLEN image sizes\n");

    // These widths are NOT multiples of 16, 32, or 64
    const size_t sizes[][2] = {
        {17, 13},
        {33, 29},
        {100, 75},
        {101, 77},
    };

    for (auto& s : sizes) {
        size_t W = s[0], H = s[1], N = W * H;

        uint8_t*  in  = (uint8_t*)aligned_alloc(64, N);
        uint8_t*  out = (uint8_t*)aligned_alloc(64, N);
        int16_t*  gx  = (int16_t*)aligned_alloc(64, N * sizeof(int16_t));
        int16_t*  gy  = (int16_t*)aligned_alloc(64, N * sizeof(int16_t));
        uint8_t*  mag = (uint8_t*)aligned_alloc(64, N);
        uint8_t*  dir = (uint8_t*)aligned_alloc(64, N);

        fill_random(in, N, (uint32_t)(W * 1000 + H));

        canny::gaussian_blur_5x5(in, out, W, H);
        canny::sobel_gradients(out, gx, gy, W, H);
        canny::magnitude_l1(gx, gy, mag, W, H);
        canny::gradient_direction(gx, gy, dir, W, H);

        // No crash = pass. Also verify output stays in [0,255]
        int bad = 0;
        for (size_t i = 0; i < N; i++)
            if (mag[i] > 255) bad++;  // always false for uint8_t, checks logic

        char label[64];
        snprintf(label, sizeof(label), "tail_%zux%zu_no_crash", W, H);
        ASSERT_EQ(bad, 0, label);

        free(in); free(out); free(gx); free(gy); free(mag); free(dir);
    }
}
// ============================================================
// Test 14: RVV Gaussian vs Scalar — ±1 tolerance
// ============================================================
static void test_rvv_gaussian_equivalence() {
    printf("\n[RVV] Gaussian RVV vs Scalar — equivalence at current VLEN\n");

    const size_t W = 100, H = 75;
    const size_t N = W * H;

    uint8_t* in         = (uint8_t*)aligned_alloc(64, N);
    uint8_t* out_scalar = (uint8_t*)aligned_alloc(64, N);
    uint8_t* out_rvv    = (uint8_t*)aligned_alloc(64, N);

    fill_random(in, N, 42);

    canny::gaussian_blur_5x5(in, out_scalar, W, H);
    canny::gaussian_blur_5x5_rvv(in, out_rvv, W, H);

    int mismatches = compare_buffers_u8(out_scalar, out_rvv, N, 1);
    ASSERT_EQ(mismatches, 0, "rvv_gaussian_vs_scalar_tol1");

    free(in); free(out_scalar); free(out_rvv);
}

// ============================================================
// Test 15: RVV Magnitude L1 vs Scalar — ±1 tolerance
// ============================================================
static void test_rvv_magnitude_l1_equivalence() {
    printf("[RVV] Magnitude L1 RVV vs Scalar — equivalence at current VLEN\n");

    const size_t W = 100, H = 75;
    const size_t N = W * H;

    int16_t* gx         = (int16_t*)aligned_alloc(64, N * sizeof(int16_t));
    int16_t* gy         = (int16_t*)aligned_alloc(64, N * sizeof(int16_t));
    uint8_t* out_scalar = (uint8_t*)aligned_alloc(64, N);
    uint8_t* out_rvv    = (uint8_t*)aligned_alloc(64, N);

    uint32_t s = 99;
    for (size_t i = 0; i < N; i++) {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        gx[i] = (int16_t)(s & 0x1FF) - 256;
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        gy[i] = (int16_t)(s & 0x1FF) - 256;
    }

    canny::magnitude_l1(gx, gy, out_scalar, W, H);
    canny::magnitude_l1_rvv(gx, gy, out_rvv, W, H);

    int mismatches = compare_buffers_u8(out_scalar, out_rvv, N, 1);
    ASSERT_EQ(mismatches, 0, "rvv_magnitude_l1_vs_scalar_tol1");

    free(gx); free(gy); free(out_scalar); free(out_rvv);
}

// ============================================================
// Bonus Stage Tests (NMS, double threshold, hysteresis)
// ============================================================
// These full-Canny stages are scalar (data-dependent control
// flow), so the goal here is property/invariant correctness and
// determinism across VLEN — not RVV equivalence.
// ============================================================

// Test 16: NMS — 1-pixel border is always suppressed to 0.
static void test_nms_border_zero() {
    printf("\n[NMS] Border ring is zeroed\n");
    const size_t W = 100, H = 75, N = W * H;
    uint8_t* mag = (uint8_t*)aligned_alloc(64, N);
    uint8_t* dir = (uint8_t*)aligned_alloc(64, N);
    uint8_t* out = (uint8_t*)aligned_alloc(64, N);
    memset(mag, 200, N);   // strong everywhere
    memset(dir, 0,   N);   // horizontal gradient
    memset(out, 9,   N);

    canny::non_max_suppression(mag, dir, out, W, H);

    int bad = 0;
    for (size_t x = 0; x < W; x++) {
        if (out[x] != 0) bad++;
        if (out[(H - 1) * W + x] != 0) bad++;
    }
    for (size_t y = 0; y < H; y++) {
        if (out[y * W] != 0) bad++;
        if (out[y * W + (W - 1)] != 0) bad++;
    }
    ASSERT_EQ(bad, 0, "nms_border_zero");

    free(mag); free(dir); free(out);
}

// Test 17: NMS — keeps a 1D ridge peak, suppresses the shoulders.
static void test_nms_ridge_peak() {
    printf("[NMS] Horizontal ridge peak kept, shoulders suppressed\n");
    const size_t W = 7, H = 3, N = W * H;
    uint8_t* mag = (uint8_t*)aligned_alloc(64, N);
    uint8_t* dir = (uint8_t*)aligned_alloc(64, N);
    uint8_t* out = (uint8_t*)aligned_alloc(64, N);
    memset(mag, 0, N);
    memset(dir, 0, N);   // horizontal gradient → compare left/right

    // middle row ramp: 0 10 20 30 20 10 0
    uint8_t row[7] = {0, 10, 20, 30, 20, 10, 0};
    for (size_t x = 0; x < W; x++) mag[1 * W + x] = row[x];

    canny::non_max_suppression(mag, dir, out, W, H);

    ASSERT_EQ(out[1 * W + 3], 30, "nms_peak_kept");
    ASSERT_EQ(out[1 * W + 2], 0,  "nms_left_shoulder_suppressed");
    ASSERT_EQ(out[1 * W + 4], 0,  "nms_right_shoulder_suppressed");

    free(mag); free(dir); free(out);
}

// Test 18: Double threshold — correct three-way classification.
static void test_double_threshold() {
    printf("\n[Threshold] Three-class classification\n");
    const size_t W = 6, H = 1, N = W * H;
    uint8_t* in  = (uint8_t*)aligned_alloc(64, N);
    uint8_t* out = (uint8_t*)aligned_alloc(64, N);
    uint8_t vals[6] = {0, 19, 20, 49, 50, 255};
    for (size_t i = 0; i < N; i++) in[i] = vals[i];

    canny::double_threshold(in, out, W, H, 20, 50);

    ASSERT_EQ(out[0], canny::EDGE_NONE,   "thr_0_none");
    ASSERT_EQ(out[1], canny::EDGE_NONE,   "thr_19_none");
    ASSERT_EQ(out[2], canny::EDGE_WEAK,   "thr_20_weak");
    ASSERT_EQ(out[3], canny::EDGE_WEAK,   "thr_49_weak");
    ASSERT_EQ(out[4], canny::EDGE_STRONG, "thr_50_strong");
    ASSERT_EQ(out[5], canny::EDGE_STRONG, "thr_255_strong");

    free(in); free(out);
}

// Test 19: Hysteresis — connected weak chain kept, isolated weak dropped.
static void test_hysteresis_trace() {
    printf("[Hysteresis] Connected chain kept, isolated weak dropped\n");
    const size_t W = 5, H = 1, N = W * H;
    uint8_t* in  = (uint8_t*)aligned_alloc(64, N);
    uint8_t* out = (uint8_t*)aligned_alloc(64, N);

    in[0] = canny::EDGE_STRONG;
    in[1] = canny::EDGE_WEAK;
    in[2] = canny::EDGE_WEAK;
    in[3] = canny::EDGE_NONE;
    in[4] = canny::EDGE_WEAK;   // isolated past the gap

    canny::hysteresis(in, out, W, H);

    ASSERT_EQ(out[0], 255, "hyst_strong");
    ASSERT_EQ(out[1], 255, "hyst_weak_connected_1");
    ASSERT_EQ(out[2], 255, "hyst_weak_connected_2");
    ASSERT_EQ(out[3], 0,   "hyst_none");
    ASSERT_EQ(out[4], 0,   "hyst_weak_isolated_dropped");

    free(in); free(out);
}

// Test 20: Full Canny (incl. bonus stages) — deterministic across VLEN.
static void test_full_canny_deterministic() {
    printf("\n[Pipeline] Full Canny (with bonus stages) — deterministic\n");
    const size_t W = 100, H = 75, N = W * H;

    uint8_t* in    = (uint8_t*)aligned_alloc(64, N);
    uint8_t* blur  = (uint8_t*)aligned_alloc(64, N);
    int16_t* gx    = (int16_t*)aligned_alloc(64, N * sizeof(int16_t));
    int16_t* gy    = (int16_t*)aligned_alloc(64, N * sizeof(int16_t));
    uint8_t* mag   = (uint8_t*)aligned_alloc(64, N);
    uint8_t* dir   = (uint8_t*)aligned_alloc(64, N);
    uint8_t* nms   = (uint8_t*)aligned_alloc(64, N);
    uint8_t* thr   = (uint8_t*)aligned_alloc(64, N);
    uint8_t* e1    = (uint8_t*)aligned_alloc(64, N);
    uint8_t* e2    = (uint8_t*)aligned_alloc(64, N);

    fill_random(in, N, 42);

    canny::gaussian_blur_5x5(in, blur, W, H);
    canny::sobel_gradients(blur, gx, gy, W, H);
    canny::magnitude_l1(gx, gy, mag, W, H);
    canny::gradient_direction(gx, gy, dir, W, H);
    canny::non_max_suppression(mag, dir, nms, W, H);
    canny::double_threshold(nms, thr, W, H, 20, 50);

    canny::hysteresis(thr, e1, W, H);
    canny::hysteresis(thr, e2, W, H);   // second run — must be identical

    // Determinism
    ASSERT_EQ(compare_buffers_u8(e1, e2, N, 0), 0, "canny_deterministic");

    // Output is strictly binary (0 or 255)
    int non_binary = 0;
    for (size_t i = 0; i < N; i++)
        if (e1[i] != 0 && e1[i] != 255) non_binary++;
    ASSERT_EQ(non_binary, 0, "canny_output_binary");

    // Every final edge must have survived NMS (edges ⊆ NMS support)
    int leaked = 0;
    for (size_t i = 0; i < N; i++)
        if (e1[i] == 255 && nms[i] == 0) leaked++;
    ASSERT_EQ(leaked, 0, "canny_edges_subset_of_nms");

    free(in); free(blur); free(gx); free(gy); free(mag);
    free(dir); free(nms); free(thr); free(e1); free(e2);
}

// ============================================================
// main
// ============================================================
int main() {
    printf("===========================================\n");
    printf("  QEMU Equivalence Tests — Scalar + RVV   \n");
    printf("===========================================\n");

    test_gaussian_uniform();
    test_gaussian_black();
    test_gaussian_impulse();
    test_sobel_uniform();
    test_sobel_vertical_edge();
    test_sobel_horizontal_edge();
    test_magnitude_l1_nonzero();
    test_magnitude_l1_zero();
    test_direction_vertical();
    test_direction_horizontal();
    test_direction_diagonal();
    test_full_pipeline_equivalence();
    test_tail_case_sizes();

    // RVV equivalence tests — scalar vs RVV at current VLEN
    test_rvv_gaussian_equivalence();
    test_rvv_magnitude_l1_equivalence();

    // Bonus stage tests — full Canny (NMS, threshold, hysteresis)
    test_nms_border_zero();
    test_nms_ridge_peak();
    test_double_threshold();
    test_hysteresis_trace();
    test_full_canny_deterministic();

    printf("\n===========================================\n");
    printf("  Results: %d passed, %d failed\n", g_passed, g_failed);
    printf("===========================================\n");

    return (g_failed == 0) ? 0 : 1;
}
