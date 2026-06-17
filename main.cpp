#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <riscv_vector.h>
#include "gaussian.hpp"
#include "sobel.hpp"
#include "magnitude.hpp"
#include "direction.hpp"
#include "nms.hpp"          // Stage 3: non-maximum suppression (bonus)
#include "threshold.hpp"    // Stages 4-5: double threshold + hysteresis (bonus)
#include "./src/profiler.hpp"

using namespace std;

// --- BARE METAL QEMU SYSCALL BYPASS ---
#define AT_FDCWD -100
#define LINUX_O_RDONLY 00
#define LINUX_O_WRONLY 01
#define LINUX_O_CREAT  0100
#define LINUX_O_TRUNC  01000

long linux_openat(const char *pathname, int flags, int mode) {
    register long a0 asm("a0") = AT_FDCWD;
    register long a1 asm("a1") = (long)pathname;
    register long a2 asm("a2") = flags;
    register long a3 asm("a3") = mode;
    register long a7 asm("a7") = 56;
    asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a7) : "memory");
    return a0;
}
long linux_read(long fd, void *buf, size_t count) {
    register long a0 asm("a0") = fd;
    register long a1 asm("a1") = (long)buf;
    register long a2 asm("a2") = count;
    register long a7 asm("a7") = 63;
    asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
    return a0;
}
long linux_write(long fd, const void *buf, size_t count) {
    register long a0 asm("a0") = fd;
    register long a1 asm("a1") = (long)buf;
    register long a2 asm("a2") = count;
    register long a7 asm("a7") = 64;
    asm volatile ("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a7) : "memory");
    return a0;
}
long linux_close(long fd) {
    register long a0 asm("a0") = fd;
    register long a7 asm("a7") = 57;
    asm volatile ("ecall" : "+r"(a0) : "r"(a7) : "memory");
    return a0;
}

// --- IMAGE I/O ---
uint8_t* load_raw_image(const char* filename, size_t width, size_t height) {
    long fd = linux_openat(filename, LINUX_O_RDONLY, 0);
    if (fd < 0) {
        printf("ERROR: Failed to open %s\n", filename);
        return nullptr;
    }
    size_t size = width * height;
    uint8_t* data = (uint8_t*)aligned_alloc(64, size);
    if (!data) { linux_close(fd); return nullptr; }
    linux_read(fd, data, size);
    linux_close(fd);
    return data;
}
void save_raw_image(const char* filename, const uint8_t* data,
                    size_t width, size_t height) {
    long fd = linux_openat(filename, LINUX_O_WRONLY | LINUX_O_CREAT | LINUX_O_TRUNC, 0666);
    if (fd >= 0) {
        linux_write(fd, data, width * height);
        linux_close(fd);
        printf("-> Saved: %s\n", filename);
    } else {
        printf("ERROR: Failed to save %s\n", filename);
    }
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        printf("Usage: %s <input_file> <width> <height> <output_file>\n", argv[0]);
        return 1;
    }

    const char* input_filename  = argv[1];
    size_t width                = atoi(argv[2]);
    size_t height               = atoi(argv[3]);
    const char* output_filename = argv[4];
    size_t total                = width * height;
    const int ITERATIONS        = 100;

    printf("--- RISC-V Canny Pipeline Start ---\n");

    // Print VLEN
    size_t vl = __riscv_vsetvl_e8m1(width);
    printf("-> Vector Length (vl for e8m1, width=%zu): %zu\n", width, vl);

    // ── Load input ────────────────────────────────────────────────
    uint8_t* input = load_raw_image(input_filename, width, height);
    if (!input) return 1;
    printf("-> Loaded: %s (%zux%zu)\n", input_filename, width, height);

    // ── Allocate all buffers ──────────────────────────────────────
    uint8_t*  blurred = (uint8_t*)aligned_alloc(64, total);
    int16_t*  Gx      = (int16_t*)aligned_alloc(64, total * sizeof(int16_t));
    int16_t*  Gy      = (int16_t*)aligned_alloc(64, total * sizeof(int16_t));
    uint8_t*  mag_l1  = (uint8_t*)aligned_alloc(64, total);
    uint8_t*  mag_l2  = (uint8_t*)aligned_alloc(64, total);
    uint8_t*  dir     = (uint8_t*)aligned_alloc(64, total);
    uint8_t*  dir_vis = (uint8_t*)aligned_alloc(64, total);
    uint8_t*  gx_vis  = (uint8_t*)aligned_alloc(64, total);
    uint8_t*  gy_vis  = (uint8_t*)aligned_alloc(64, total);
    uint8_t*  nms     = (uint8_t*)aligned_alloc(64, total);  // Stage 3 (bonus)
    uint8_t*  thresh  = (uint8_t*)aligned_alloc(64, total);  // Stage 4 (bonus)
    uint8_t*  edges   = (uint8_t*)aligned_alloc(64, total);  // Stage 5 (bonus)

    // ── Timing accumulators ───────────────────────────────────────
    uint64_t t_start, t_end;
    uint64_t acc_gaussian_scalar = 0;
    uint64_t acc_gaussian_rvv    = 0;
    uint64_t acc_sobel           = 0;
    uint64_t acc_mag_l1_scalar   = 0;
    uint64_t acc_mag_l1_rvv      = 0;
    uint64_t acc_mag_l2          = 0;
    uint64_t acc_dir             = 0;
    uint64_t acc_nms             = 0;   // Stage 3 (bonus)
    uint64_t acc_thresh          = 0;   // Stage 4 (bonus)
    uint64_t acc_hyst            = 0;   // Stage 5 (bonus)

    printf("-> Running %d iterations for stable timing...\n", ITERATIONS);

    // ── Timing loop ───────────────────────────────────────────────
    for (int iter = 0; iter < ITERATIONS; iter++) {

        // Stage 1a: Gaussian scalar
        t_start = now_ns();
        canny::gaussian_blur_5x5(input, blurred, width, height);
        t_end = now_ns();
        acc_gaussian_scalar += (t_end - t_start);

        // Stage 1b: Gaussian RVV
        // Uses scalar for border + RVV for interior (strip-mining, LMUL=2)
        t_start = now_ns();
        canny::gaussian_blur_5x5_rvv(input, blurred, width, height);
        t_end = now_ns();
        acc_gaussian_rvv += (t_end - t_start);

        // Stage 2: Sobel (scalar only — only 4% of time, not worth RVV)
        t_start = now_ns();
        canny::sobel_gradients(blurred, Gx, Gy, width, height);
        t_end = now_ns();
        acc_sobel += (t_end - t_start);

        // Stage 3a: Magnitude L1 scalar
        t_start = now_ns();
        canny::magnitude_l1(Gx, Gy, mag_l1, width, height);
        t_end = now_ns();
        acc_mag_l1_scalar += (t_end - t_start);

        // Stage 3b: Magnitude L1 RVV
        // Uses vneg+vmax for abs, vadd for sum, vredmax for global max
        t_start = now_ns();
        canny::magnitude_l1_rvv(Gx, Gy, mag_l1, width, height);
        t_end = now_ns();
        acc_mag_l1_rvv += (t_end - t_start);

        // Stage 4: Magnitude L2 (scalar — sqrt not vectorizable)
        t_start = now_ns();
        double max_mag = 0.0;
        for (size_t i = 0; i < total; i++) {
            double m = sqrt((double)Gx[i]*Gx[i] + (double)Gy[i]*Gy[i]);
            if (m > max_mag) max_mag = m;
        }
        for (size_t i = 0; i < total; i++) {
            double m = sqrt((double)Gx[i]*Gx[i] + (double)Gy[i]*Gy[i]);
            mag_l2[i] = (max_mag > 0) ? (uint8_t)((m * 255.0) / max_mag) : 0;
        }
        t_end = now_ns();
        acc_mag_l2 += (t_end - t_start);

        // Stage 5: Direction (scalar)
        t_start = now_ns();
        canny::gradient_direction(Gx, Gy, dir, width, height);
        t_end = now_ns();
        acc_dir += (t_end - t_start);

        // ── Bonus stages: full Canny (3=NMS, 4=threshold, 5=hysteresis) ──
        // Inherently data-dependent / branchy → scalar by design.

        // Stage 3 (bonus): Non-maximum suppression — thin edges to 1px
        t_start = now_ns();
        canny::non_max_suppression(mag_l1, dir, nms, width, height);
        t_end = now_ns();
        acc_nms += (t_end - t_start);

        // Stage 4 (bonus): Double threshold → strong / weak / none
        t_start = now_ns();
        canny::double_threshold(nms, thresh, width, height, 20, 50);
        t_end = now_ns();
        acc_thresh += (t_end - t_start);

        // Stage 5 (bonus): Hysteresis — connect weak edges to strong ones
        t_start = now_ns();
        canny::hysteresis(thresh, edges, width, height);
        t_end = now_ns();
        acc_hyst += (t_end - t_start);
    }

    // ── Scalar timing table ───────────────────────────────────────
    printf("\n--- Scalar Pipeline ---\n");
    StageTiming scalar_stages[] = {
        {"Gaussian 5x5",  0, ns_to_ms(acc_gaussian_scalar / ITERATIONS), 0},
        {"Sobel Gx/Gy",   0, ns_to_ms(acc_sobel           / ITERATIONS), 0},
        {"Magnitude L1",  0, ns_to_ms(acc_mag_l1_scalar   / ITERATIONS), 0},
        {"Magnitude L2",  0, ns_to_ms(acc_mag_l2           / ITERATIONS), 0},
        {"Direction",     0, ns_to_ms(acc_dir              / ITERATIONS), 0},
        {"NMS (bonus)",       0, ns_to_ms(acc_nms    / ITERATIONS), 0},
        {"Threshold (bonus)", 0, ns_to_ms(acc_thresh / ITERATIONS), 0},
        {"Hysteresis (bonus)",0, ns_to_ms(acc_hyst   / ITERATIONS), 0},
    };
    print_timing_table(scalar_stages, 8, ITERATIONS, 0);

    // ── RVV timing table ──────────────────────────────────────────
    printf("\n--- RVV Pipeline ---\n");
    StageTiming rvv_stages[] = {
        {"Gaussian RVV",     0, ns_to_ms(acc_gaussian_rvv  / ITERATIONS), 0},
        {"Sobel Gx/Gy",      0, ns_to_ms(acc_sobel         / ITERATIONS), 0},
        {"Magnitude L1 RVV", 0, ns_to_ms(acc_mag_l1_rvv    / ITERATIONS), 0},
        {"Magnitude L2",     0, ns_to_ms(acc_mag_l2         / ITERATIONS), 0},
        {"Direction",        0, ns_to_ms(acc_dir            / ITERATIONS), 0},
    };
    print_timing_table(rvv_stages, 5, ITERATIONS, 0);

    // ── Speedup summary ───────────────────────────────────────────
    double gaussian_speedup = ns_to_ms(acc_gaussian_scalar / ITERATIONS) /
                              ns_to_ms(acc_gaussian_rvv    / ITERATIONS);
    double mag_l1_speedup   = ns_to_ms(acc_mag_l1_scalar   / ITERATIONS) /
                              ns_to_ms(acc_mag_l1_rvv      / ITERATIONS);

    printf("=== RVV Speedup Summary ===\n");
    printf("Gaussian  scalar: %.4f ms  RVV: %.4f ms  speedup: %.2fx\n",
           ns_to_ms(acc_gaussian_scalar / ITERATIONS),
           ns_to_ms(acc_gaussian_rvv    / ITERATIONS),
           gaussian_speedup);
    printf("Mag L1    scalar: %.4f ms  RVV: %.4f ms  speedup: %.2fx\n",
           ns_to_ms(acc_mag_l1_scalar / ITERATIONS),
           ns_to_ms(acc_mag_l1_rvv   / ITERATIONS),
           mag_l1_speedup);
    printf("===========================\n\n");

    // ── Save outputs (SCALAR pipeline for reference matching) ────
canny::gaussian_blur_5x5(input, blurred, width, height);
canny::sobel_gradients(blurred, Gx, Gy, width, height);
canny::magnitude_l1(Gx, Gy, mag_l1, width, height);

// Recompute Magnitude L2 from scalar Gx/Gy
double max_mag = 0.0;
for (size_t i = 0; i < total; i++) {
    double m = sqrt((double)Gx[i]*Gx[i] + (double)Gy[i]*Gy[i]);
    if (m > max_mag) max_mag = m;
}
for (size_t i = 0; i < total; i++) {
    double m = sqrt((double)Gx[i]*Gx[i] + (double)Gy[i]*Gy[i]);
    mag_l2[i] = (max_mag > 0) ? (uint8_t)((m * 255.0) / max_mag) : 0;
}

// Recompute direction from scalar Gx/Gy
canny::gradient_direction(Gx, Gy, dir, width, height);

    save_raw_image("Output_Images/output_gaussian.raw", blurred, width, height);

    for (size_t i = 0; i < total; i++) {
        int32_t ax = Gx[i] < 0 ? -Gx[i] : Gx[i];
        int32_t ay = Gy[i] < 0 ? -Gy[i] : Gy[i];
        gx_vis[i] = (uint8_t)(ax > 255 ? 255 : ax);
        gy_vis[i] = (uint8_t)(ay > 255 ? 255 : ay);
    }
    save_raw_image("Output_Images/output_sobel_gx.raw", gx_vis, width, height);
    save_raw_image("Output_Images/output_sobel_gy.raw", gy_vis, width, height);
    save_raw_image("Output_Images/output_magnitude_l1.raw", mag_l1, width, height);
    save_raw_image(output_filename, mag_l2, width, height);
    for (size_t i = 0; i < total; i++) dir_vis[i] = dir[i] * 85;
    save_raw_image("Output_Images/output_direction.raw", dir_vis, width, height);

    // ── Bonus stages: run on the final L1 magnitude and save each ──
    canny::non_max_suppression(mag_l1, dir, nms, width, height);
    canny::double_threshold(nms, thresh, width, height, 20, 50);
    canny::hysteresis(thresh, edges, width, height);
    save_raw_image("Output_Images/output_nms.raw",       nms,    width, height);
    save_raw_image("Output_Images/output_threshold.raw", thresh, width, height);
    save_raw_image("Output_Images/output_edges.raw",     edges,  width, height);

    // ── Cleanup ───────────────────────────────────────────────────
    free(input); free(blurred);
    free(Gx); free(Gy);
    free(mag_l1); free(mag_l2);
    free(dir); free(dir_vis);
    free(gx_vis); free(gy_vis);
    free(nms); free(thresh); free(edges);

    printf("--- RISC-V Canny Pipeline End ---\n");
    return 0;
}