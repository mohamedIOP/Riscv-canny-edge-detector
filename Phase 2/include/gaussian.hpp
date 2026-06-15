#ifndef GAUSSIAN_HPP
#define GAUSSIAN_HPP

#include <cstdint>
#include <cstddef>

namespace canny {

// 5x5 Gaussian kernel coefficients (sigma ≈ 1.0).
// Stored row-major. Sum of all coefficients = 273.
extern const int16_t GAUSSIAN_5X5[25];

// Applies a 5x5 Gaussian blur to a grayscale image.
// Uses zero-padding at the boundaries.
// Both buffers must be at least width*height bytes.
void gaussian_blur_5x5(const uint8_t* in, uint8_t* out,
                       size_t width, size_t height);
// RVV (vectorized) version. Processes interior pixels only —
// the caller must copy/zero the 2-pixel border separately if needed.
// Uses LMUL=1 for the int32 accumulator (vint32m2_t internally).
void gaussian_blur_5x5_rvv(const uint8_t* in, uint8_t* out,
                           size_t width, size_t height);

}  // namespace canny

#endif
