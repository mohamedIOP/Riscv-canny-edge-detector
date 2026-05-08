#include "sobel.hpp"

namespace canny {

const int16_t SOBEL_X[9] = {
    -1, 0, 1,
    -2, 0, 2,
    -1, 0, 1
};

const int16_t SOBEL_Y[9] = {
    -1, -2, -1,
     0,  0,  0,
     1,  2,  1
};

void sobel_gradients(const uint8_t* in,
                     int16_t* gx_out,
                     int16_t* gy_out,
                     size_t width,
                     size_t height) {

    const int radius = 1;

    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {

            int32_t sum_x = 0;
            int32_t sum_y = 0;

            for (int ky = -radius; ky <= radius; ky++) {
                for (int kx = -radius; kx <= radius; kx++) {

                    int iy = y + ky;
                    int ix = x + kx;

                    uint8_t pixel = 0;

                    if (ix >= 0 && ix < (int)width &&
                        iy >= 0 && iy < (int)height) {
                        pixel = in[iy * width + ix];
                    }

                    int k_idx = (ky + radius) * 3 + (kx + radius);

                    sum_x += pixel * SOBEL_X[k_idx];
                    sum_y += pixel * SOBEL_Y[k_idx];
                }
            }

            gx_out[y * width + x] = sum_x;
            gy_out[y * width + x] = sum_y;
        }
    }
}

}
