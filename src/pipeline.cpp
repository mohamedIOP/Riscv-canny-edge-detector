#include "pipeline.hpp"
#include "gaussian.hpp"
#include "sobel.hpp"
template <typename PixelType, typename AccumType>
void convolve2D(
    const PixelType* input,
    PixelType* output,
    int width,
    int height,
    const float* kernel,
    int kernelSize
)
{
}

void gaussianBlur(
    const uint8_t* input,
    uint8_t* output,
    int width,
    int height
)
{
    for (int i = 0; i < width * height; i++) {
        output[i] = input[i];
    }
}

void sobelGradient(
    const uint8_t* input,
    int16_t* Gx,
    int16_t* Gy,
    int width,
    int height
)
{
    for (int i = 0; i < width * height; i++) {
        Gx[i] = 0;
        Gy[i] = 0;
    }
}
