#include <riscv_vector.h>
#include <cstdint>
#include <cstddef>
#include <cstdio>

// 1x5 horizontal kernel: [1, 4, 7, 4, 1], sum = 17
void blur_1x5_rvv(const uint8_t* in, uint8_t* out, size_t width) {
    const int16_t kernel[5] = {1, 4, 7, 4, 1};
    const int radius = 2;

    for (size_t x = radius; x < width - radius; ) {
        size_t remaining = (width - radius) - x;
        size_t vl = __riscv_vsetvl_e16m1(remaining);

        // accumulator (int16): kernel weights are small,
        // so the sum fits comfortably in int16
        vint16m1_t sum = __riscv_vmv_v_x_i16m1(0, vl);

        for (int k = -radius; k <= radius; k++) {
            int16_t coeff = kernel[k + radius];

            // load uint8 pixels and widen them to int16
            vuint8mf2_t pixels_u8 = __riscv_vle8_v_u8mf2(&in[x + k], vl);
            vint16m1_t pixels_i16 = __riscv_vreinterpret_v_u16m1_i16m1(
                __riscv_vwcvtu_x_x_v_u16m1(pixels_u8, vl));

            // multiply by coefficient and accumulate
            sum = __riscv_vadd_vv_i16m1(sum,
                    __riscv_vmul_vx_i16m1(pixels_i16, coeff, vl), vl);
        }

        // divide by kernel sum (17)
        vint16m1_t result = __riscv_vdiv_vx_i16m1(sum, 17, vl);

        // convert back to uint8 and store
        vuint8mf2_t out_u8 = __riscv_vnclipu_wx_u8mf2(
            __riscv_vreinterpret_v_i16m1_u16m1(result), 0, __RISCV_VXRM_RNU, vl);
        __riscv_vse8_v_u8mf2(&out[x], out_u8, vl);

        x += vl;
    }
}

int main() {
    const size_t width = 20;
    uint8_t in[width], out[width];
    for (size_t i = 0; i < width; i++) in[i] = 100;

    blur_1x5_rvv(in, out, width);

    for (size_t i = 0; i < width; i++)
        printf("out[%zu] = %d\n", i, out[i]);

    return 0;
}
