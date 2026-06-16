#include <riscv_vector.h>
#include <cstdint>
#include <cstddef>
#include <cstdio>

void vector_add_u8(const uint8_t* a, const uint8_t* b, 
                    uint8_t* out, size_t n) {
    for (size_t i = 0; i < n; ) {
        size_t vl = __riscv_vsetvl_e8m1(n - i);
        printf("  i=%zu, vl=%zu\n", i, vl);  // <-- ضيف ده

        vuint8m1_t va = __riscv_vle8_v_u8m1(&a[i], vl);
        vuint8m1_t vb = __riscv_vle8_v_u8m1(&b[i], vl);
        vuint8m1_t vc = __riscv_vadd_vv_u8m1(va, vb, vl);
        __riscv_vse8_v_u8m1(&out[i], vc, vl);

        i += vl;
    }
}
int main() {
    const size_t n = 20;
    uint8_t a[n], b[n], out[n];

    for (size_t i = 0; i < n; i++) {
        a[i] = (uint8_t)(i);
        b[i] = (uint8_t)(100);
    }

    vector_add_u8(a, b, out, n);

    printf("Result:\n");
    for (size_t i = 0; i < n; i++) {
        printf("out[%zu] = %d (expected %d)\n", i, out[i], (int)a[i] + 100);
    }

    return 0;
}
