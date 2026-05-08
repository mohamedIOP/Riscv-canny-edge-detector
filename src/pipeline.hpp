#pragma once

#include <cstdint>

template <typename PixelType, typename AccumType>
void convolve2D(
    const PixelType* input,
    PixelType* output,
    int width,
    int height,
    const float* kernel,
    int kernelSize
);

void gaussianBlur(
    const uint8_t* input,
    uint8_t* output,
    int width,
    int height
);

void sobelGradient(
    const uint8_t* input,
    int16_t* Gx,
    int16_t* Gy,
    int width,
    int height
);
