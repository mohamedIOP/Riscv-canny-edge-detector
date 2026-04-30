#include "direction.hpp"

namespace canny {

static inline int32_t abs32(int32_t v) {
    return v < 0 ? -v : v;
}

void gradient_direction(const int16_t* gx, const int16_t* gy,
                        uint8_t* dir_out, size_t width, size_t height) {
    size_t total = width * height;
    
    for (size_t i = 0; i < total; i++) {
        int32_t x = gx[i];
        int32_t y = gy[i];
        int32_t ax = abs32(x);
        int32_t ay = abs32(y);
        
        // Compare against tan(22.5°) ≈ 2/5 and tan(67.5°) ≈ 12/5
        // Cross-multiplication avoids floating-point division:
        //   ay/ax < 2/5  ⇔  5*ay < 2*ax
        //   ay/ax < 12/5 ⇔  5*ay < 12*ax
        
        if (5 * ay < 2 * ax) {
            dir_out[i] = 0;       // horizontal gradient → 0°
        }
        else if (5 * ay >= 12 * ax) {
            dir_out[i] = 2;       // vertical gradient → 90°
        }
        else {
            // Diagonal region: distinguish 45° from 135° by sign
            if ((x >= 0 && y >= 0) || (x < 0 && y < 0)) {
                dir_out[i] = 1;   // 45°
            } else {
                dir_out[i] = 3;   // 135°
            }
        }
    }
}

}  // namespace canny