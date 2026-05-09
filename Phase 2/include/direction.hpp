#ifndef DIRECTION_HPP
#define DIRECTION_HPP

#include <cstdint>
#include <cstddef>

namespace canny {

// Quantized gradient direction (4 levels):
//   0 = 0°    (horizontal gradient, vertical edge)
//   1 = 45°   (diagonal)
//   2 = 90°   (vertical gradient, horizontal edge)
//   3 = 135°  (anti-diagonal)
//
// Uses integer cross-multiplication instead of atan2()
// for performance on embedded targets.
void gradient_direction(const int16_t* gx, const int16_t* gy,
                        uint8_t* dir_out, size_t width, size_t height);

}  // namespace canny

#endif