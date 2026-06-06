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
void save_raw_image(const char* filename, const uint8_t* data, size_t width, size_t height) {
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

    printf("--- RISC-V Canny Pipeline Start ---\n");

    // Print VLEN
    size_t vl = __riscv_vsetvl_e8m1(width);
    printf("-> Vector Length (vl for e8m1, width=%zu): %zu\n", width, vl);

    // ── Step 1: Load input ────────────────────────────────────────────────
    uint8_t* input = load_raw_image(input_filename, width, height);
    if (!input) return 1;
    printf("-> Loaded: %s (%zux%zu)\n", input_filename, width, height);

    // ── Step 2: Gaussian Blur ─────────────────────────────────────────────
    // Reduces noise before edge detection.
    // Uses a 5x5 kernel with integer coefficients summing to 273.
    uint8_t* blurred = (uint8_t*)aligned_alloc(64, total);
    canny::gaussian_blur_5x5(input, blurred, width, height);
    save_raw_image("Output_Images/output_gaussian.raw", blurred, width, height);
    printf("-> Gaussian blur done\n");

    // ── Step 3: Sobel Gradients ───────────────────────────────────────────
    // Detects intensity changes in X and Y directions separately.
    // Stored as int16_t SoA (separate Gx and Gy arrays) for efficient
    // vector loading later in the RVV optimization phase.
    int16_t* Gx = (int16_t*)aligned_alloc(64, total * sizeof(int16_t));
    int16_t* Gy = (int16_t*)aligned_alloc(64, total * sizeof(int16_t));
    canny::sobel_gradients(blurred, Gx, Gy, width, height);

    // Convert Gx and Gy to uint8 for visualization (clamp abs value to 255)
    uint8_t* gx_vis = (uint8_t*)aligned_alloc(64, total);
    uint8_t* gy_vis = (uint8_t*)aligned_alloc(64, total);
    for (size_t i = 0; i < total; i++) {
        int32_t ax = Gx[i] < 0 ? -Gx[i] : Gx[i];
        int32_t ay = Gy[i] < 0 ? -Gy[i] : Gy[i];
        gx_vis[i] = (uint8_t)(ax > 255 ? 255 : ax);
        gy_vis[i] = (uint8_t)(ay > 255 ? 255 : ay);
    }
    save_raw_image("Output_Images/output_sobel_gx.raw", gx_vis, width, height);
    save_raw_image("Output_Images/output_sobel_gy.raw", gy_vis, width, height);
    free(gx_vis);
    free(gy_vis);
    printf("-> Sobel gradients done\n");

    // ── Step 4: Magnitude L1 ─────────────────────────────────────────────
    // L1 = |Gx| + |Gy|, normalized to [0,255].
    // Fast integer-only approximation. Slight overestimate on diagonals.
    uint8_t* mag_l1 = (uint8_t*)aligned_alloc(64, total);
    canny::magnitude_l1(Gx, Gy, mag_l1, width, height);
    save_raw_image("Output_Images/output_magnitude_l1.raw", mag_l1, width, height);
    printf("-> Magnitude L1 done\n");

    // ── Step 5: Magnitude L2 ─────────────────────────────────────────────
    // L2 = sqrt(Gx² + Gy²), normalized to [0,255].
    // Mathematically correct but requires floating point.
    // Two-pass: find max first, then normalize (single-pass not straightforward
    // because normalization factor depends on the global maximum).
    uint8_t* mag_l2 = (uint8_t*)aligned_alloc(64, total);
    double max_mag = 0.0;
    for (size_t i = 0; i < total; i++) {
        double m = sqrt((double)Gx[i]*Gx[i] + (double)Gy[i]*Gy[i]);
        if (m > max_mag) max_mag = m;
    }
    for (size_t i = 0; i < total; i++) {
        double m = sqrt((double)Gx[i]*Gx[i] + (double)Gy[i]*Gy[i]);
        mag_l2[i] = (max_mag > 0) ? (uint8_t)((m * 255.0) / max_mag) : 0;
    }
    save_raw_image(output_filename, mag_l2, width, height);  // main output
    printf("-> Magnitude L2 done (saved as main output: %s)\n", output_filename);

    // ── Step 6: Gradient Direction ────────────────────────────────────────
    // Quantizes angle to 4 directions: 0=horizontal, 1=diagonal,
    // 2=vertical, 3=anti-diagonal.
    // Uses integer cross-multiplication instead of atan2() — embedded trick.
    // Values are 0/1/2/3 — multiply by 85 to make visible (0/85/170/255).
    uint8_t* dir     = (uint8_t*)aligned_alloc(64, total);
    uint8_t* dir_vis = (uint8_t*)aligned_alloc(64, total);
    canny::gradient_direction(Gx, Gy, dir, width, height);
    for (size_t i = 0; i < total; i++) {
        dir_vis[i] = dir[i] * 85;  // scale 0-3 → 0/85/170/255 for visibility
    }
    save_raw_image("Output_Images/output_direction.raw", dir_vis, width, height);
    printf("-> Gradient direction done\n");

    // ── Cleanup ───────────────────────────────────────────────────────────
    free(input);
    free(blurred);
    free(Gx);
    free(Gy);
    free(mag_l1);
    free(mag_l2);
    free(dir);
    free(dir_vis);

    printf("--- RISC-V Canny Pipeline End ---\n");
    return 0;
}