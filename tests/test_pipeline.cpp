#include <gtest/gtest.h>
#include "../src/pipeline.hpp"
#include <vector>
#include <cstdint>

TEST(SanityCheck, OnePlusOne) {
    EXPECT_EQ(1 + 1, 2);
}

TEST(GaussianBlur, UniformImageStaysUniform) {
    int width = 10;
    int height = 10;

    std::vector<uint8_t> input(width * height, 100);
    std::vector<uint8_t> output(width * height, 0);
    gaussianBlur(
        input.data(),
        output.data(),
        width,
        height
    );

    for (int y = 2; y < height - 2; y++) {
        for (int x = 2; x < width - 2; x++) {
            int i = y * width + x;
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
    sobelGradient(
        input.data(),
        Gx.data(),
        Gy.data(),
        width,
        height
    );

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            int i = y * width + x;
            EXPECT_EQ(Gx[i], 0);
            EXPECT_EQ(Gy[i], 0);
        }
    }
}
