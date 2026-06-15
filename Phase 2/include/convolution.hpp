#pragma once
#include <cstdint>
#include <cstddef>

namespace canny {

// Generic 2D convolution template
// PixelT  = Pixel type (uint8_t for images)
// AccumT  = Accumulator type (int32_t to prevent overflow)
// KernelT = Kernel weights type (int16_t)

template<typename PixelT, typename AccumT, typename KernelT>
void convolve(
    const PixelT* input,
    PixelT*       output,
    int           width,
    int           height,
    const KernelT* kernel,
    int           ksize,
    AccumT        divisor)
{
    int radius = ksize / 2;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            AccumT sum = 0;

            for (int ky = -radius; ky <= radius; ky++) {
                for (int kx = -radius; kx <= radius; kx++) {
                    int iy = y + ky;
                    int ix = x + kx;

                    PixelT pixel = 0;
                    if (ix >= 0 && ix < width && iy >= 0 && iy < height)
                        pixel = input[iy * width + ix];

                    int k_idx = (ky + radius) * ksize + (kx + radius);
                    sum += (AccumT)pixel * (AccumT)kernel[k_idx];
                }
            }

            sum /= divisor;
            if (sum < 0)   sum = 0;
            if (sum > 255) sum = 255;
            output[y * width + x] = (PixelT)sum;
        }
    }
}

} // namespace canny
