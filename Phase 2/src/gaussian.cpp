#include "../include/gaussian.hpp"
#include <riscv_vector.h>
#include "../include/convolution.hpp"

namespace canny {

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
// RVV (vectorized) Gaussian blur — interior pixels only.
// Uses the same algorithm as gaussian_blur_5x5 but processes
// VLEN-agnostic vector chunks via RVV intrinsics.
void gaussian_blur_5x5_rvv(const uint8_t* in, uint8_t* out,
                            size_t width, size_t height) {
    const int radius = 2;
    const int16_t normalizer = 273;

    // process interior rows only
    for (size_t y = radius; y < height - radius; y++) {

        // strip-mine across interior columns
        for (size_t x = radius; x < width - radius; ) {
            size_t remaining = (width - radius) - x;
            size_t vl = __riscv_vsetvl_e16m1(remaining);

            // accumulator: int32, widened from int16 (LMUL=2)
            // max possible sum = 255*273 ≈ 69615, overflows int16
            vint32m2_t sum = __riscv_vmv_v_x_i32m2(0, vl);

            for (int ky = -radius; ky <= radius; ky++) {
                for (int kx = -radius; kx <= radius; kx++) {
                    int16_t coeff = GAUSSIAN_5X5[(ky + radius) * 5 + (kx + radius)];

                    size_t offset = (y + ky) * width + (x + kx);

                    // load uint8 pixels (mf2 = half register width, since we widen 2x)
                    vuint8mf2_t pixels_u8 = __riscv_vle8_v_u8mf2(&in[offset], vl);

                    // widen uint8 -> uint16 (m1) -> int16
                    vuint16m1_t pixels_u16 = __riscv_vwcvtu_x_x_v_u16m1(pixels_u8, vl);
                    vint16m1_t pixels_i16 = __riscv_vreinterpret_v_u16m1_i16m1(pixels_u16);

                    // widen int16 -> int32 (m2), multiply by coefficient, accumulate
                    vint32m2_t pixels_i32 = __riscv_vwcvt_x_x_v_i32m2(pixels_i16, vl);
                    vint32m2_t term = __riscv_vmul_vx_i32m2(pixels_i32, (int32_t)coeff, vl);

                    sum = __riscv_vadd_vv_i32m2(sum, term, vl);
                }
            }

            // divide by 273
            vint32m2_t result32 = __riscv_vdiv_vx_i32m2(sum, normalizer, vl);

            // narrow int32 -> int16 -> uint8, with clamping to [0,255]
            vint16m1_t result16 = __riscv_vnclip_wx_i16m1(result32, 0, __RISCV_VXRM_RNU, vl);
            vuint8mf2_t out_u8 = __riscv_vnclipu_wx_u8mf2(
                __riscv_vreinterpret_v_i16m1_u16m1(result16), 0, __RISCV_VXRM_RNU, vl);

            __riscv_vse8_v_u8mf2(&out[y * width + x], out_u8, vl);

            x += vl;
        }
    }
}

} // namespace canny
