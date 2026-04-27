#ifndef MAGNITUDE_HPP
#define MAGNITUDE_HPP

#include <cstdint>
#include <cstddef>

namespace canny {

// L1 norm: |Gx| + |Gy|, normalized to [0, 255].
// Uses two passes: one to find max, one to normalize.
void magnitude_l1(const int16_t* gx, const int16_t* gy,
                  uint8_t* mag_out, size_t width, size_t height);

}  // namespace canny

#endif