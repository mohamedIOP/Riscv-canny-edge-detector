#include "magnitude.hpp"
#include <cmath>
#include <cstdlib>
#ifdef __riscv
#include <riscv_vector.h>
#endif

namespace canny {

static inline int32_t abs32(int32_t v) {
    return v < 0 ? -v : v;
}

void magnitude_l1(const int16_t* gx, const int16_t* gy,
                  uint8_t* mag_out, size_t width, size_t height) {
    size_t total = width * height;
    
    // Pass 1: find the maximum |Gx| + |Gy|
    int32_t max_mag = 0;
    for (size_t i = 0; i < total; i++) {
        int32_t m = abs32(gx[i]) + abs32(gy[i]);
        if (m > max_mag) max_mag = m;
    }
    
    if (max_mag == 0) {
        // All gradients are zero — output black image
        for (size_t i = 0; i < total; i++) mag_out[i] = 0;
        return;
    }
    
    // Pass 2: normalize to [0, 255]
    for (size_t i = 0; i < total; i++) {
        int32_t m = abs32(gx[i]) + abs32(gy[i]);
        mag_out[i] = (uint8_t)((m * 255) / max_mag);
    }
}
void magnitude_l2(const int16_t* gx, const int16_t* gy,
                  uint8_t* mag_out, size_t width, size_t height) {
    size_t total = width * height;

    // Pass 1: find maximum magnitude
    double max_mag = 0.0;
    for (size_t i = 0; i < total; i++) {
        double m = sqrt((double)gx[i]*gx[i] + (double)gy[i]*gy[i]);
        if (m > max_mag) max_mag = m;
    }

    if (max_mag == 0.0) {
        for (size_t i = 0; i < total; i++) mag_out[i] = 0;
        return;
    }

    // Pass 2: normalize to [0, 255]
    for (size_t i = 0; i < total; i++) {
        double m = sqrt((double)gx[i]*gx[i] + (double)gy[i]*gy[i]);
        mag_out[i] = (uint8_t)((m * 255.0) / max_mag);
    }
}
#ifdef __riscv
void magnitude_l1_rvv(const int16_t* gx, const int16_t* gy,
                      uint8_t* mag_out, size_t width, size_t height) {
    size_t n = width * height;

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

        running_max = __riscv_vredmax_vs_i16m1_i16m1(sum, running_max, vl);

        i += vl;
    }

    int16_t max_val = __riscv_vmv_x_s_i16m1_i16(running_max);

    if (max_val == 0) {
        for (size_t i = 0; i < n; i++) mag_out[i] = 0;
        free(raw);
        return;
    }

    for (size_t i = 0; i < n; ) {
        size_t vl = __riscv_vsetvl_e16m1(n - i);

        vint16m1_t v = __riscv_vle16_v_i16m1(&raw[i], vl);

        vint32m2_t v32 = __riscv_vwcvt_x_x_v_i32m2(v, vl);
        vint32m2_t scaled = __riscv_vmul_vx_i32m2(v32, 255, vl);
        vint32m2_t normalized = __riscv_vdiv_vx_i32m2(scaled, (int32_t)max_val, vl);

        vint16m1_t result16 = __riscv_vnclip_wx_i16m1(normalized, 0, __RISCV_VXRM_RNU, vl);
        vuint8mf2_t result_u8 = __riscv_vnclipu_wx_u8mf2(
            __riscv_vreinterpret_v_i16m1_u16m1(result16), 0, __RISCV_VXRM_RNU, vl);

        __riscv_vse8_v_u8mf2(&mag_out[i], result_u8, vl);

        i += vl;
    }

    free(raw);
}
#endif
}  // namespace canny
