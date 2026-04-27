#include "magnitude.hpp"

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

}  // namespace canny