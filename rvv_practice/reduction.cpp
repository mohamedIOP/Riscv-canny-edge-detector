#include <riscv_vector.h>
#include <cstdint>
#include <cstddef>
#include <cstdio>

// Find the maximum value in an array using RVV reduction
int16_t find_max_rvv(const int16_t* data, size_t n) {
    // start with the smallest possible int16 value
    vint16m1_t running_max = __riscv_vmv_v_x_i16m1(INT16_MIN, 1);

    for (size_t i = 0; i < n; ) {
        size_t vl = __riscv_vsetvl_e16m1(n - i);

        vint16m1_t v = __riscv_vle16_v_i16m1(&data[i], vl);

        // reduction: find max across this chunk, combined with running_max
        // result is written to element 0 of running_max
        running_max = __riscv_vredmax_vs_i16m1_i16m1(v, running_max, vl);

        i += vl;
    }

    // extract the scalar result from element 0
    return __riscv_vmv_x_s_i16m1_i16(running_max);
}

int main() {
    int16_t data[7] = {10, 250, -5, 99, 300, 7, 1};
    int16_t result = find_max_rvv(data, 7);
    printf("max = %d (expected 300)\n", result);
    return 0;
}
