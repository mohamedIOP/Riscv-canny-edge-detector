#include <riscv_vector.h>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

namespace canny {

const int16_t GAUSSIAN_5X5[25] = {
    1,  4,  7,  4, 1,
    4, 16, 26, 16, 4,
    7, 26, 41, 26, 7,
    4, 16, 26, 16, 4,
    1,  4,  7,  4, 1
};

// RVV Gaussian 5x5 — interior pixels only (no boundary handling yet)
// LMUL=1 (m1): each vector variable uses 1 physical register
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

            // accumulator (int16): max possible sum is 255*273 ≈ 69615,
            // which OVERFLOWS int16 (max 32767)! We need int32 here.
            // -> Use int32m2 accumulator instead (widened LMUL).
            vint32m2_t sum = __riscv_vmv_v_x_i32m2(0, vl);

            for (int ky = -radius; ky <= radius; ky++) {
                for (int kx = -radius; kx <= radius; kx++) {
                    int16_t coeff = GAUSSIAN_5X5[(ky + radius) * 5 + (kx + radius)];

                    size_t offset = (y + ky) * width + (x + kx);

                    // load uint8 pixels (mf2 = half register, since we'll widen 2x)
                    vuint8mf2_t pixels_u8 = __riscv_vle8_v_u8mf2(&in[offset], vl);

                    // widen uint8 -> uint16 (m1)
                    vuint16m1_t pixels_u16 = __riscv_vwcvtu_x_x_v_u16m1(pixels_u8, vl);
                    vint16m1_t pixels_i16 = __riscv_vreinterpret_v_u16m1_i16m1(pixels_u16);

                    // widen int16 -> int32 (m2), then multiply by coefficient
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

// ============================================================
// Test harness
// ============================================================
int main() {
    const size_t W = 16, H = 16;
    uint8_t in[W * H], out[W * H];

    // Test 1: uniform image (all 128) -> output should stay ~128
    memset(in, 128, W * H);
    memset(out, 0, W * H);
    canny::gaussian_blur_5x5_rvv(in, out, W, H);

    printf("Test: uniform image (128) -> blur output (interior pixels)\n");
    int bad = 0;
    for (size_t y = 2; y < H - 2; y++) {
        for (size_t x = 2; x < W - 2; x++) {
            int v = out[y * W + x];
            if (v < 127 || v > 129) bad++;
        }
    }
    printf("  mismatches (expected ~128): %d\n", bad);
    printf("  sample out[5*W+5] = %d\n", out[5 * W + 5]);
// Test 2: impulse response — single bright pixel in dark image
    memset(in, 0, W * H);
    memset(out, 0, W * H);
    in[8 * W + 8] = 255;  // center pixel = 255

    canny::gaussian_blur_5x5_rvv(in, out, W, H);

    printf("\nTest: impulse response\n");
    int center = out[8 * W + 8];
    int left   = out[8 * W + 7];
    int right  = out[8 * W + 9];
    int up     = out[7 * W + 8];
    int down   = out[9 * W + 8];

    printf("  center=%d, left=%d, right=%d, up=%d, down=%d\n",
           center, left, right, up, down);
    printf("  center > neighbors? %s\n",
           (center > left && center > right && center > up && center > down) ? "YES" : "NO");
    printf("  left == right? %s\n", (left == right) ? "YES" : "NO");
    printf("  up == down? %s\n", (up == down) ? "YES" : "NO");

    return 0;
}
