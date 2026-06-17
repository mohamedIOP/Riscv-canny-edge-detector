#include "../include/gaussian.hpp"
#ifdef __riscv
#include <riscv_vector.h>
#endif
#include "../include/convolution.hpp"
#include <cstdlib>

namespace canny {

// ── 2D 5x5 kernel ────────────────────────────────────────────────────────
const int16_t GAUSSIAN_5X5[25] = {
     1,  4,  7,  4, 1,
     4, 16, 26, 16, 4,
     7, 26, 41, 26, 7,
     4, 16, 26, 16, 4,
     1,  4,  7,  4, 1
};

// ── Scalar 2D convolution ─────────────────────────────────────────────────
void gaussian_blur_5x5(const uint8_t* in, uint8_t* out,
                       size_t width, size_t height) {
    convolve<uint8_t, int32_t, int16_t>(
        in, out,
        (int)width, (int)height,
        GAUSSIAN_5X5, 5,
        273
    );
}

// ── Separable version: 1x5 pass then 5x1 pass ────────────────────────────
static const int16_t GAUSSIAN_1D[5] = { 1, 4, 7, 4, 1 };
static const int     GAUSSIAN_1D_SUM = 17;

void gaussian_blur_5x5_separable(const uint8_t* in, uint8_t* out,
                                  size_t width, size_t height) {
    int w = (int)width;
    int h = (int)height;

    uint8_t* temp = (uint8_t*)aligned_alloc(64, width * height);

    // Pass 1: Horizontal (1x5)
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int32_t sum = 0;
            for (int kx = -2; kx <= 2; kx++) {
                int ix = x + kx;
                uint8_t pixel = 0;
                if (ix >= 0 && ix < w)
                    pixel = in[y * w + ix];
                sum += (int32_t)pixel * (int32_t)GAUSSIAN_1D[kx + 2];
            }
            sum /= GAUSSIAN_1D_SUM;
            if (sum < 0)   sum = 0;
            if (sum > 255) sum = 255;
            temp[y * w + x] = (uint8_t)sum;
        }
    }

    // Pass 2: Vertical (5x1)
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int32_t sum = 0;
            for (int ky = -2; ky <= 2; ky++) {
                int iy = y + ky;
                uint8_t pixel = 0;
                if (iy >= 0 && iy < h)
                    pixel = temp[iy * w + x];
                sum += (int32_t)pixel * (int32_t)GAUSSIAN_1D[ky + 2];
            }
            sum /= GAUSSIAN_1D_SUM;
            if (sum < 0)   sum = 0;
            if (sum > 255) sum = 255;
            out[y * w + x] = (uint8_t)sum;
        }
    }

    free(temp);
}

#ifdef __riscv
// ── RVV Gaussian blur ─────────────────────────────────────────────────────
// Strategy:
//   - Border pixels (2-pixel ring): computed with scalar (boundary-safe)
//   - Interior pixels: computed with RVV strip-mining (no boundary check)
//
// Key optimization: replace vdiv by 273 with fixed-point multiply-shift.
//   1/273 ≈ 240/65536  (error < 0.02%, safe for uint8 output)
//   From the hints guide: "(sum * 240) >> 16 approximates sum / 273"
//   This eliminates slow vector integer division entirely.
//
// LMUL chain (chosen LMUL=2 from LMUL sweep results):
//   uint8  mf2  → load vl pixels
//   uint16 m1   → widen (LMUL doubles: mf2 → m1)
//   uint32 m2   → widen again for multiply (m1 → m2)
//   uint32 m2   → multiply by coefficient, accumulate
//   uint32 m2   → multiply by 240, shift right 16 (normalize)
//   uint16 m1   → narrow (m2 → m1)
//   uint8  mf2  → narrow (m1 → mf2), store

void gaussian_blur_5x5_rvv(const uint8_t* in, uint8_t* out,
                            size_t width, size_t height) {
    const int radius = 2;

    // ── Scalar border: top 2 rows ─────────────────────────────────
    for (size_t y = 0; y < (size_t)radius; y++) {
        for (size_t x = 0; x < width; x++) {
            int32_t sum = 0;
            for (int ky = -radius; ky <= radius; ky++)
                for (int kx = -radius; kx <= radius; kx++) {
                    int iy = (int)y + ky, ix = (int)x + kx;
                    uint8_t p = 0;
                    if (iy >= 0 && iy < (int)height &&
                        ix >= 0 && ix < (int)width)
                        p = in[iy * width + ix];
                    sum += (int32_t)p * GAUSSIAN_5X5[(ky+radius)*5+(kx+radius)];
                }
            sum = (sum * 240) >> 16;
            if (sum > 255) sum = 255;
            out[y * width + x] = (uint8_t)sum;
        }
    }

    // ── Scalar border: bottom 2 rows ──────────────────────────────
    for (size_t y = height - radius; y < height; y++) {
        for (size_t x = 0; x < width; x++) {
            int32_t sum = 0;
            for (int ky = -radius; ky <= radius; ky++)
                for (int kx = -radius; kx <= radius; kx++) {
                    int iy = (int)y + ky, ix = (int)x + kx;
                    uint8_t p = 0;
                    if (iy >= 0 && iy < (int)height &&
                        ix >= 0 && ix < (int)width)
                        p = in[iy * width + ix];
                    sum += (int32_t)p * GAUSSIAN_5X5[(ky+radius)*5+(kx+radius)];
                }
            sum = (sum * 240) >> 16;
            if (sum > 255) sum = 255;
            out[y * width + x] = (uint8_t)sum;
        }
    }

    // ── Interior rows: scalar left/right border + RVV interior ────
    for (size_t y = radius; y < height - radius; y++) {

        // Left 2 columns (scalar)
        for (int bx = 0; bx < radius; bx++) {
            int32_t sum = 0;
            for (int ky = -radius; ky <= radius; ky++)
                for (int kx = -radius; kx <= radius; kx++) {
                    int iy = (int)y + ky, ix = bx + kx;
                    uint8_t p = 0;
                    if (iy >= 0 && iy < (int)height &&
                        ix >= 0 && ix < (int)width)
                        p = in[iy * width + ix];
                    sum += (int32_t)p * GAUSSIAN_5X5[(ky+radius)*5+(kx+radius)];
                }
            sum = (sum * 240) >> 16;
            if (sum > 255) sum = 255;
            out[y * width + bx] = (uint8_t)sum;
        }

        // Right 2 columns (scalar)
        for (int bx = 0; bx < radius; bx++) {
            int rx = (int)width - radius + bx;
            int32_t sum = 0;
            for (int ky = -radius; ky <= radius; ky++)
                for (int kx = -radius; kx <= radius; kx++) {
                    int iy = (int)y + ky, ix = rx + kx;
                    uint8_t p = 0;
                    if (iy >= 0 && iy < (int)height &&
                        ix >= 0 && ix < (int)width)
                        p = in[iy * width + ix];
                    sum += (int32_t)p * GAUSSIAN_5X5[(ky+radius)*5+(kx+radius)];
                }
            sum = (sum * 240) >> 16;
            if (sum > 255) sum = 255;
            out[y * width + rx] = (uint8_t)sum;
        }

        // RVV interior columns (no boundary check needed)
        for (size_t x = radius; x < width - radius; ) {
            size_t remaining = (width - radius) - x;

            // Set vector length for uint16 m1
            // (mf2 for uint8 load, m1 for uint16, m2 for uint32 accumulator)
            size_t vl = __riscv_vsetvl_e16m1(remaining);

            // Initialize uint32 accumulator to zero
            vuint32m2_t sum = __riscv_vmv_v_x_u32m2(0, vl);

            for (int ky = -radius; ky <= radius; ky++) {
                for (int kx = -radius; kx <= radius; kx++) {
                    // Kernel coefficient (always positive in Gaussian)
                    uint32_t coeff = (uint32_t)GAUSSIAN_5X5[(ky+radius)*5+(kx+radius)];
                    size_t offset  = (y + ky) * width + (x + kx);

                    // Load uint8 pixels — no boundary check (interior only)
                    vuint8mf2_t pixels_u8 = __riscv_vle8_v_u8mf2(&in[offset], vl);

                    // Widen uint8 → uint16 (LMUL: mf2 → m1)
                    vuint16m1_t pixels_u16 = __riscv_vwcvtu_x_x_v_u16m1(pixels_u8, vl);

                    // Widen uint16 → uint32 (LMUL: m1 → m2)
                    vuint32m2_t pixels_u32 = __riscv_vwcvtu_x_x_v_u32m2(pixels_u16, vl);

                    // Multiply by kernel coefficient and accumulate
                    vuint32m2_t term = __riscv_vmul_vx_u32m2(pixels_u32, coeff, vl);
                    sum = __riscv_vadd_vv_u32m2(sum, term, vl);
                }
            }

            // Fixed-point normalize: (sum * 240) >> 16 ≈ sum / 273
            // Replaces slow vdiv with fast vmul + vsrl
            vuint32m2_t scaled  = __riscv_vmul_vx_u32m2(sum, 240, vl);
            vuint32m2_t shifted = __riscv_vsrl_vx_u32m2(scaled, 16, vl);

            // Narrow uint32 → uint16 (LMUL: m2 → m1) with saturation
            vuint16m1_t result16 = __riscv_vnclipu_wx_u16m1(shifted, 0, __RISCV_VXRM_RNU, vl);

            // Narrow uint16 → uint8 (LMUL: m1 → mf2) with saturation
            vuint8mf2_t out_u8 = __riscv_vnclipu_wx_u8mf2(result16, 0, __RISCV_VXRM_RNU, vl);

            // Store result
            __riscv_vse8_v_u8mf2(&out[y * width + x], out_u8, vl);
            x += vl;
        }
    }
}
#endif

} // namespace canny