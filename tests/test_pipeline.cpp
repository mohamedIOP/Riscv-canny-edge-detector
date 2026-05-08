#include <gtest/gtest.h>

#include "../src/pipeline.hpp"
#include "../src/gaussian.hpp"
#include "../src/sobel.hpp"

#include <vector>

TEST(SanityCheck, OnePlusOne) {
    EXPECT_EQ(1 + 1, 2);
}

TEST(GaussianBlur, UniformImageStaysUniform) {

    int width = 10;
    int height = 10;

    std::vector<uint8_t> input(width * height, 100);
    std::vector<uint8_t> output(width * height, 0);

    canny::gaussian_blur_5x5(
        input.data(),
        output.data(),
        width,
        height
    );

    for (size_t y = 2; y < height - 2; y++) {
        for (size_t x = 2; x < width - 2; x++) {
            size_t i = y * width + x;
            EXPECT_EQ(static_cast<int>(output[i]), 100);
        }
    }
}

TEST(SobelGradient, UniformImageHasZeroGradient) {

    int width = 10;
    int height = 10;

    std::vector<uint8_t> input(width * height, 128);
    std::vector<int16_t> Gx(width * height, 0);
    std::vector<int16_t> Gy(width * height, 0);

    canny::sobel_gradients(
        input.data(),
        Gx.data(),
        Gy.data(),
        width,
        height
    );

    for (size_t y = 2; y < height - 2; y++) {
        for (size_t x = 2; x < width - 2; x++) {
            size_t i = y * width + x;
            EXPECT_EQ(Gx[i], 0);
            EXPECT_EQ(Gy[i], 0);
        }
    }
}
