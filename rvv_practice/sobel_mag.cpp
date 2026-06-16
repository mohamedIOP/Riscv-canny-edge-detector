#include <riscv_vector.h>
#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include <cstdio>

// L1 magnitude (unnormalized): |Gx| + |Gy|
// Output is int16 (could exceed 255 before normalization)
void magnitude_l1_raw_rvv(const int16_t* gx, const int16_t* gy,
                           int16_t* out, size_t n) {
    for (size_t i = 0; i < n; ) {
        size_t vl = __riscv_vsetvl_e16m1(n - i);

        vint16m1_t vgx = __riscv_vle16_v_i16m1(&gx[i], vl);
        vint16m1_t vgy = __riscv_vle16_v_i16m1(&gy[i], vl);

        // absolute value: max(x, -x)
        vint16m1_t abs_gx = __riscv_vmax_vv_i16m1(vgx, __riscv_vneg_v_i16m1(vgx, vl), vl);
        vint16m1_t abs_gy = __riscv_vmax_vv_i16m1(vgy, __riscv_vneg_v_i16m1(vgy, vl), vl);

        vint16m1_t sum = __riscv_vadd_vv_i16m1(abs_gx, abs_gy, vl);

        __riscv_vse16_v_i16m1(&out[i], sum, vl);

        i += vl;
    }
}

int main() {
    const size_t n = 5;
    int16_t gx[n] = {100, -50, 0, -200, 30};
    int16_t gy[n] = {-100, 50, 0, 100, -10};
    int16_t out[n];

    magnitude_l1_raw_rvv(gx, gy, out, n);

    for (size_t i = 0; i < n; i++) {
        int expected = abs(gx[i]) + abs(gy[i]);
        printf("out[%zu] = %d (expected %d)\n", i, out[i], expected);
    }

    return 0;
}
