#include "../include/gaussian.hpp"
#include "../include/convolution.hpp"
#include <cstdlib>

namespace canny {

// ── 2D 5x5 kernel (original) ─────────────────────────────────────────────
const int16_t GAUSSIAN_5X5[25] = {
     1,  4,  7,  4, 1,
     4, 16, 26, 16, 4,
     7, 26, 41, 26, 7,
     4, 16, 26, 16, 4,
     1,  4,  7,  4, 1
};

void gaussian_blur_5x5(const uint8_t* in, uint8_t* out,
                       size_t width, size_t height) {
    convolve<uint8_t, int32_t, int16_t>(
        in, out,
        (int)width, (int)height,
        GAUSSIAN_5X5, 5,
        273
    );
}

// ── Separable version: 1x5 pass then 5x1 pass ────────────────────────────
static const int16_t GAUSSIAN_1D[5] = { 1, 4, 7, 4, 1 };
static const int     GAUSSIAN_1D_SUM = 17;

void gaussian_blur_5x5_separable(const uint8_t* in, uint8_t* out,
                                  size_t width, size_t height) {
    int w = (int)width;
    int h = (int)height;

    uint8_t* temp = (uint8_t*)aligned_alloc(64, width * height);

    // Pass 1: Horizontal (1x5)
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int32_t sum = 0;
            for (int kx = -2; kx <= 2; kx++) {
                int ix = x + kx;
                uint8_t pixel = 0;
                if (ix >= 0 && ix < w)
                    pixel = in[y * w + ix];
                sum += (int32_t)pixel * (int32_t)GAUSSIAN_1D[kx + 2];
            }
            sum /= GAUSSIAN_1D_SUM;
            if (sum < 0)   sum = 0;
            if (sum > 255) sum = 255;
            temp[y * w + x] = (uint8_t)sum;
        }
    }

    // Pass 2: Vertical (5x1)
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int32_t sum = 0;
            for (int ky = -2; ky <= 2; ky++) {
                int iy = y + ky;
                uint8_t pixel = 0;
                if (iy >= 0 && iy < h)
                    pixel = temp[iy * w + x];
                sum += (int32_t)pixel * (int32_t)GAUSSIAN_1D[ky + 2];
            }
            sum /= GAUSSIAN_1D_SUM;
            if (sum < 0)   sum = 0;
            if (sum > 255) sum = 255;
            out[y * w + x] = (uint8_t)sum;
        }
    }

    free(temp);
}

} // namespace canny