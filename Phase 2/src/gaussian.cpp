#include "../include/gaussian.hpp"
#include "../include/convolution.hpp"

namespace canny {

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

} // namespace canny
