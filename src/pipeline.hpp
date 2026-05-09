#pragma once
#include <cstdint>
#include <cstddef>
#include "../Phase 2/include/gaussian.hpp"
#include "../Phase 2/include/sobel.hpp"


// Generic convolution template interface
// PixelType = uint8_t for grayscale images
// AccumType = int32_t or float to avoid overflow during math
template <typename PixelType, typename AccumType>
void convolve2D(
    const PixelType* input,
    PixelType*       output,
    int              width,
    int              height,
    const float*     kernel,
    int              kernelSize
);
