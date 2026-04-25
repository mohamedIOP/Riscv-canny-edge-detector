#ifndef SOBEL_HPP
#define SOBEL_HPP

#include <cstdint>
#include <cstddef>

namespace canny {

extern const int16_t SOBEL_X[9];
extern const int16_t SOBEL_Y[9];

// Computes Sobel X and Y gradients in a single pass.
// gx_out and gy_out must each hold width*height int16_t values.
void sobel_gradients(const uint8_t* in,
                     int16_t* gx_out, int16_t* gy_out,
                     size_t width, size_t height);

}  // namespace canny

#endif