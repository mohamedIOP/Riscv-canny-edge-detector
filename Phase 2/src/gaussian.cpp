#include "../include/gaussian.hpp"
#ifdef __riscv
#include <riscv_vector.h>
#endif
#include "../include/convolution.hpp"
#include <cstdlib>

namespace canny {

// ── 2D 5x5 kernel (original) ─────────────────────────────────────────────
const int16_t GAUSSIAN_5X5[25] = {
     1,  4,  7,  4, 1,
     4, 16, 26, 16, 4,
     7, 26, 41, 26, 7,
     4, 16, 26, 16, 4,
     1,  4,  7,  4, 1
};

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
// RVV (vectorized) Gaussian blur — interior pixels only.
void gaussian_blur_5x5_rvv(const uint8_t* in, uint8_t* out,
                            size_t width, size_t height) {
    const int radius = 2;
    const int16_t normalizer = 273;

    for (size_t y = radius; y < height - radius; y++) {
        for (size_t x = radius; x < width - radius; ) {
            size_t remaining = (width - radius) - x;
            size_t vl = __riscv_vsetvl_e16m1(remaining);

            vint32m2_t sum = __riscv_vmv_v_x_i32m2(0, vl);

            for (int ky = -radius; ky <= radius; ky++) {
                for (int kx = -radius; kx <= radius; kx++) {
                    int16_t coeff = GAUSSIAN_5X5[(ky + radius) * 5 + (kx + radius)];
                    size_t offset = (y + ky) * width + (x + kx);

                    vuint8mf2_t pixels_u8 = __riscv_vle8_v_u8mf2(&in[offset], vl);
                    vuint16m1_t pixels_u16 = __riscv_vwcvtu_x_x_v_u16m1(pixels_u8, vl);
                    vint16m1_t pixels_i16 = __riscv_vreinterpret_v_u16m1_i16m1(pixels_u16);
                    vint32m2_t pixels_i32 = __riscv_vwcvt_x_x_v_i32m2(pixels_i16, vl);
                    vint32m2_t term = __riscv_vmul_vx_i32m2(pixels_i32, (int32_t)coeff, vl);

                    sum = __riscv_vadd_vv_i32m2(sum, term, vl);
                }
            }

            vint32m2_t result32 = __riscv_vdiv_vx_i32m2(sum, normalizer, vl);
            vint16m1_t result16 = __riscv_vnclip_wx_i16m1(result32, 0, __RISCV_VXRM_RNU, vl);
            vuint8mf2_t out_u8 = __riscv_vnclipu_wx_u8mf2(
                __riscv_vreinterpret_v_i16m1_u16m1(result16), 0, __RISCV_VXRM_RNU, vl);

            __riscv_vse8_v_u8mf2(&out[y * width + x], out_u8, vl);
            x += vl;
        }
    }
}
#endif
} // namespace canny
