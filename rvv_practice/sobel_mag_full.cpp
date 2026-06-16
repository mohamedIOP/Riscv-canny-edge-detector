#include <riscv_vector.h>
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <cstdio>

namespace canny {

// RVV Sobel magnitude L1: |Gx| + |Gy|, normalized to [0, 255]
void magnitude_l1_rvv(const int16_t* gx, const int16_t* gy,
                      uint8_t* mag_out, size_t width, size_t height) {
    size_t n = width * height;

    // ---------- Pass 1: compute |Gx|+|Gy| into a temp buffer, find max ----------
    // (in the real project this temp buffer could be reused / allocated by caller)
    int16_t* raw = (int16_t*)malloc(n * sizeof(int16_t));

    vint16m1_t running_max = __riscv_vmv_v_x_i16m1(0, 1);

    for (size_t i = 0; i < n; ) {
        size_t vl = __riscv_vsetvl_e16m1(n - i);

        vint16m1_t vgx = __riscv_vle16_v_i16m1(&gx[i], vl);
        vint16m1_t vgy = __riscv_vle16_v_i16m1(&gy[i], vl);

        vint16m1_t abs_gx = __riscv_vmax_vv_i16m1(vgx, __riscv_vneg_v_i16m1(vgx, vl), vl);
        vint16m1_t abs_gy = __riscv_vmax_vv_i16m1(vgy, __riscv_vneg_v_i16m1(vgy, vl), vl);

        vint16m1_t sum = __riscv_vadd_vv_i16m1(abs_gx, abs_gy, vl);

        __riscv_vse16_v_i16m1(&raw[i], sum, vl);

        // running max across all chunks
        running_max = __riscv_vredmax_vs_i16m1_i16m1(sum, running_max, vl);

        i += vl;
    }

    int16_t max_val = __riscv_vmv_x_s_i16m1_i16(running_max);
    if (max_val == 0) max_val = 1;  // avoid division by zero

    // ---------- Pass 2: normalize to [0, 255] ----------
    for (size_t i = 0; i < n; ) {
        size_t vl = __riscv_vsetvl_e16m1(n - i);

        vint16m1_t v = __riscv_vle16_v_i16m1(&raw[i], vl);

        // widen to int32 for the multiply (v * 255 can exceed int16 range)
        vint32m2_t v32 = __riscv_vwcvt_x_x_v_i32m2(v, vl);
        vint32m2_t scaled = __riscv_vmul_vx_i32m2(v32, 255, vl);
        vint32m2_t normalized = __riscv_vdiv_vx_i32m2(scaled, (int32_t)max_val, vl);

        // narrow int32 -> int16 -> uint8
        vint16m1_t result16 = __riscv_vnclip_wx_i16m1(normalized, 0, __RISCV_VXRM_RNU, vl);
        vuint8mf2_t result_u8 = __riscv_vnclipu_wx_u8mf2(
            __riscv_vreinterpret_v_i16m1_u16m1(result16), 0, __RISCV_VXRM_RNU, vl);

        __riscv_vse8_v_u8mf2(&mag_out[i], result_u8, vl);

        i += vl;
    }

    free(raw);
}

} // namespace canny

// ============================================================
// Test harness
// ============================================================
int main() {
    const size_t n = 6;
    int16_t gx[n] = {100, -50, 0, -200, 30, 256};
    int16_t gy[n] = {-100, 50, 0, 100, -10, 0};
    uint8_t mag[n];

    canny::magnitude_l1_rvv(gx, gy, mag, n, 1);

    // max raw value = |256|+|0| = 256, so 256/256*255 = 255
    int16_t raw[n];
    for (size_t i = 0; i < n; i++)
        raw[i] = abs(gx[i]) + abs(gy[i]);

    int16_t max_raw = 0;
    for (size_t i = 0; i < n; i++)
        if (raw[i] > max_raw) max_raw = raw[i];

    printf("max_raw = %d\n", max_raw);
    for (size_t i = 0; i < n; i++) {
        int expected = (raw[i] * 255) / max_raw;
        printf("mag[%zu] = %d (expected ~%d, raw=%d)\n", i, mag[i], expected, raw[i]);
    }

    return 0;
}
