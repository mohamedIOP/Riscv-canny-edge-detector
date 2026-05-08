#include "gaussian.hpp"

namespace canny {

const int16_t GAUSSIAN_5X5[25] = {
    1,  4,  7,  4, 1,
    4, 16, 26, 16, 4,
    7, 26, 41, 26, 7,
    4, 16, 26, 16, 4,
    1,  4,  7,  4, 1
};

void gaussian_blur_5x5(const uint8_t* in, uint8_t* out,
                       size_t width, size_t height) {

    const int radius = 2;
    const int normalizer = 273;

    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {

            int32_t sum = 0;

            for (int ky = -radius; ky <= radius; ky++) {
                for (int kx = -radius; kx <= radius; kx++) {

                    int iy = (int)y + ky;
                    int ix = (int)x + kx;

                    uint8_t pixel = 0;

                    if (ix >= 0 && ix < (int)width &&
                        iy >= 0 && iy < (int)height) {
                        pixel = in[iy * width + ix];
                    }

                    int k_idx = (ky + radius) * 5 + (kx + radius);
                    sum += pixel * GAUSSIAN_5X5[k_idx];
                }
            }

            int32_t result = sum / normalizer;

            if (result < 0) result = 0;
            if (result > 255) result = 255;

            out[y * width + x] = (uint8_t)result;
        }
    }
}

}
