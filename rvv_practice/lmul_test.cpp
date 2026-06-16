#include <riscv_vector.h>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>

extern const int16_t GAUSSIAN_5X5[25] = {
    1,  4,  7,  4, 1,
    4, 16, 26, 16, 4,
    7, 26, 41, 26, 7,
    4, 16, 26, 16, 4,
    1,  4,  7,  4, 1
};

// ============================================================
// LMUL=1 version (mf4 load -> m1 accumulator: smallest configuration)
// ============================================================
void gaussian_lmul1(const uint8_t* in, uint8_t* out, size_t width, size_t height) {
    const int radius = 2;
    const int16_t normalizer = 273;

    for (size_t y = radius; y < height - radius; y++) {
        for (size_t x = radius; x < width - radius; ) {
            size_t remaining = (width - radius) - x;
            size_t vl = __riscv_vsetvl_e16mf2(remaining);

            vint32m1_t sum = __riscv_vmv_v_x_i32m1(0, vl);

            for (int ky = -radius; ky <= radius; ky++) {
                for (int kx = -radius; kx <= radius; kx++) {
                    int16_t coeff = GAUSSIAN_5X5[(ky + radius) * 5 + (kx + radius)];
                    size_t offset = (y + ky) * width + (x + kx);

                    vuint8mf4_t pixels_u8 = __riscv_vle8_v_u8mf4(&in[offset], vl);
                    vuint16mf2_t pixels_u16 = __riscv_vwcvtu_x_x_v_u16mf2(pixels_u8, vl);
                    vint16mf2_t pixels_i16 = __riscv_vreinterpret_v_u16mf2_i16mf2(pixels_u16);
                    vint32m1_t pixels_i32 = __riscv_vwcvt_x_x_v_i32m1(pixels_i16, vl);
                    vint32m1_t term = __riscv_vmul_vx_i32m1(pixels_i32, (int32_t)coeff, vl);

                    sum = __riscv_vadd_vv_i32m1(sum, term, vl);
                }
            }

            vint32m1_t result32 = __riscv_vdiv_vx_i32m1(sum, normalizer, vl);
            vint16mf2_t result16 = __riscv_vnclip_wx_i16mf2(result32, 0, __RISCV_VXRM_RNU, vl);
            vuint8mf4_t out_u8 = __riscv_vnclipu_wx_u8mf4(
                __riscv_vreinterpret_v_i16mf2_u16mf2(result16), 0, __RISCV_VXRM_RNU, vl);

            __riscv_vse8_v_u8mf4(&out[y * width + x], out_u8, vl);
            x += vl;
        }
    }
}

// ============================================================
// LMUL=2 version (current implementation: mf2 load -> m2 accumulator)
// ============================================================
void gaussian_lmul2(const uint8_t* in, uint8_t* out, size_t width, size_t height) {
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

// ============================================================
// LMUL=4 version (m1 load -> m4 accumulator: process more elements per chunk)
// ============================================================
void gaussian_lmul4(const uint8_t* in, uint8_t* out, size_t width, size_t height) {
    const int radius = 2;
    const int16_t normalizer = 273;

    for (size_t y = radius; y < height - radius; y++) {
        for (size_t x = radius; x < width - radius; ) {
            size_t remaining = (width - radius) - x;
            size_t vl = __riscv_vsetvl_e16m2(remaining);

            vint32m4_t sum = __riscv_vmv_v_x_i32m4(0, vl);

            for (int ky = -radius; ky <= radius; ky++) {
                for (int kx = -radius; kx <= radius; kx++) {
                    int16_t coeff = GAUSSIAN_5X5[(ky + radius) * 5 + (kx + radius)];
                    size_t offset = (y + ky) * width + (x + kx);

                    vuint8m1_t pixels_u8 = __riscv_vle8_v_u8m1(&in[offset], vl);
                    vuint16m2_t pixels_u16 = __riscv_vwcvtu_x_x_v_u16m2(pixels_u8, vl);
                    vint16m2_t pixels_i16 = __riscv_vreinterpret_v_u16m2_i16m2(pixels_u16);
                    vint32m4_t pixels_i32 = __riscv_vwcvt_x_x_v_i32m4(pixels_i16, vl);
                    vint32m4_t term = __riscv_vmul_vx_i32m4(pixels_i32, (int32_t)coeff, vl);

                    sum = __riscv_vadd_vv_i32m4(sum, term, vl);
                }
            }

            vint32m4_t result32 = __riscv_vdiv_vx_i32m4(sum, normalizer, vl);
            vint16m2_t result16 = __riscv_vnclip_wx_i16m2(result32, 0, __RISCV_VXRM_RNU, vl);
            vuint8m1_t out_u8 = __riscv_vnclipu_wx_u8m1(
                __riscv_vreinterpret_v_i16m2_u16m2(result16), 0, __RISCV_VXRM_RNU, vl);

            __riscv_vse8_v_u8m1(&out[y * width + x], out_u8, vl);
            x += vl;
        }
    }
}

// ============================================================
// Timing harness (uses clock() — works on bare-metal Newlib)
// ============================================================
double time_it(void (*fn)(const uint8_t*, uint8_t*, size_t, size_t),
                const uint8_t* in, uint8_t* out, size_t W, size_t H, int iters) {
    clock_t start = clock();
    for (int i = 0; i < iters; i++) fn(in, out, W, H);
    clock_t end = clock();
    return (double)(end - start) / CLOCKS_PER_SEC;
}

int main() {
    const size_t W = 256, H = 256;
    uint8_t* in  = new uint8_t[W * H];
    uint8_t* out = new uint8_t[W * H];

    for (size_t i = 0; i < W * H; i++) in[i] = (uint8_t)(i % 256);

    const int iters = 100;

    double t1 = time_it(gaussian_lmul1, in, out, W, H, iters);
    double t2 = time_it(gaussian_lmul2, in, out, W, H, iters);
    double t4 = time_it(gaussian_lmul4, in, out, W, H, iters);

    printf("LMUL=1: %.6f sec total, %.6f sec/iter\n", t1, t1 / iters);
    printf("LMUL=2: %.6f sec total, %.6f sec/iter\n", t2, t2 / iters);
    printf("LMUL=4: %.6f sec total, %.6f sec/iter\n", t4, t4 / iters);

    delete[] in;
    delete[] out;
    return 0;
}
