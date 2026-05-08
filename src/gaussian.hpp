#ifndef GAUSSIAN_HPP
#define GAUSSIAN_HPP

#include <cstdint>
#include <cstddef>

namespace canny {

extern const int16_t GAUSSIAN_5X5[25];

void gaussian_blur_5x5(const uint8_t* in,
                        uint8_t* out,
                        size_t width,
                        size_t height);

}

#endif
