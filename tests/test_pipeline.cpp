#include <gtest/gtest.h>
#include "../src/pipeline.hpp"
#include "../Phase 2/include/gaussian.hpp"
#include "../Phase 2/include/sobel.hpp"
#include <vector>
#include <cmath>
#include <algorithm>
using namespace std;

// ================================================================
// SANITY CHECK
// Just verifies that GoogleTest is working correctly.
// Has nothing to do with image processing.
// ================================================================
TEST(SanityCheck, OnePlusOne) {
    EXPECT_EQ(1 + 1, 2);  // if this fails, GoogleTest itself is broken
}

// ================================================================
// GAUSSIAN BLUR TESTS
// ================================================================

// TEST: UniformImageStaysUniform
// --------------------------------
// IDEA: If every pixel in the image has the same value (128),
// then after Gaussian blur, the interior pixels must still be 128.
// Because blurring averages a pixel with its neighbors —
// if all neighbors are equal, the average equals the same value.
//
// NOTE: We skip the border pixels (first/last 2 rows and columns)
// because the 5x5 kernel has radius=2, and border pixels
// get averaged with zeros outside the image (zero-padding),
// which makes them slightly less than 128. That is correct behavior.
TEST(GaussianBlur, UniformImageStaysUniform) {
    int width = 10, height = 10;

    // Create input image: all pixels = 128
    vector<uint8_t> input(width * height, 128);

    // Create output image: all zeros (will be filled by blur)
    vector<uint8_t> output(width * height, 0);

    // Apply Gaussian blur from Student B's implementation
    canny::gaussian_blur_5x5(
        input.data(),   // pointer to input array
        output.data(),  // pointer to output array
        width,
        height
    );

    // Check only interior pixels (skip 2-pixel border)
    // y goes from 2 to height-3, x goes from 2 to width-3
    for (int y = 2; y < height - 2; y++) {
        for (int x = 2; x < width - 2; x++) {
            // Convert 2D position (y,x) to 1D index: y * width + x
            EXPECT_NEAR(output[y * width + x], 128,1); // allow +-1 rounding
        }
    }
}
//all-black image must stay all-black after blur
TEST(GaussianBlur, AllBlackStaysBlack) {
    int width = 10, height = 10;

    vector<uint8_t> input(width * height, 0);
    vector<uint8_t> output(width * height, 0);

    canny::gaussian_blur_5x5(
        input.data(),
        output.data(),
        width,
        height
    );

    // no border skipping needed — black * any weight = black
    for (int i = 0; i < width * height; i++) {
        EXPECT_EQ(output[i], 0);
    }
}
// TEST: ImpulseResponse
// --------------------------------
// IDEA: Place one bright pixel (255) in the center of a dark image (all 0s).
// After Gaussian blur, the bright pixel should "spread" to its neighbors
// like a bell curve. The center must remain the brightest point.
// This verifies the blur is actually spreading values, not just copying.
TEST(GaussianBlur, ImpulseResponse) {
    int width = 7, height = 7;

    // All pixels = 0 (dark image)
    vector<uint8_t> input(width * height, 0);
    vector<uint8_t> output(width * height, 0);

    // Set center pixel (row=3, col=3) to 255 (bright)
    // Formula: row * width + col = 3 * 7 + 3 = 24
    input[3 * width + 3] = 255;

    canny::gaussian_blur_5x5(
        input.data(),
        output.data(),
        width,
        height
    );

    // After blur, center must still be brighter than all 4 direct neighbors
    // EXPECT_GT(a, b) means: expect a > b
    EXPECT_GT(output[3 * width + 3], output[3 * width + 2]); // center > left
    EXPECT_GT(output[3 * width + 3], output[3 * width + 4]); // center > right
    EXPECT_GT(output[3 * width + 3], output[2 * width + 3]); // center > above
    EXPECT_GT(output[3 * width + 3], output[4 * width + 3]); // center > below
    //check symmetry — left must equal right, above must equal below
    EXPECT_EQ(output[3 * width + 2], output[3 * width + 4]); // left == right
    EXPECT_EQ(output[2 * width + 3], output[4 * width + 3]); // above == below
}

// ================================================================
// SOBEL GRADIENT TESTS
// ================================================================

// TEST: UniformImageHasZeroGradient
// --------------------------------
// IDEA: A completely flat image (all pixels = 128) has no edges.
// The Sobel operator measures how much pixel values CHANGE.
// If nothing changes → gradient must be zero everywhere.
//
// NOTE: We skip the border pixels (first/last 1 row and column)
// because the 3x3 kernel has radius=1, and border pixels
// see zeros outside the image, which creates fake gradients.
// Interior pixels only see real neighbors → gradient = 0.
TEST(SobelGradient, UniformImageHasZeroGradient) {
    int width = 10, height = 10;

    // Flat image: every pixel = 128
    vector<uint8_t> input(width * height, 128);

    // Gx = horizontal gradient (detects vertical edges)
    // Gy = vertical gradient   (detects horizontal edges)
    vector<int16_t> Gx(width * height, 0);
    vector<int16_t> Gy(width * height, 0);

    canny::sobel_gradients(
        input.data(),
        Gx.data(),
        Gy.data(),
        width,
        height
    );

    // Check only interior pixels (skip 1-pixel border)
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            EXPECT_EQ(Gx[y * width + x], 0); // no horizontal change
            EXPECT_EQ(Gy[y * width + x], 0); // no vertical change
        }
    }
}

// TEST: VerticalEdgeDetected
// --------------------------------
// IDEA: Create an image where the left half is black (0)
// and the right half is white (255). This creates a VERTICAL edge.
// The Sobel Gx kernel detects HORIZONTAL changes (left-right differences).
// So at the edge, Gx must be much larger than Gy.
TEST(SobelGradient, VerticalEdgeDetected) {
    int width = 7, height = 7;

    // All pixels start as black (0)
    vector<uint8_t> input(width * height, 0);

    // Set right half (columns 4,5,6) to white (255)
    // This creates a vertical edge between columns 3 and 4
    for (int r = 0; r < height; r++)
        for (int c = 4; c < width; c++)
            input[r * width + c] = 255;

    vector<int16_t> Gx(width * height, 0);
    vector<int16_t> Gy(width * height, 0);

    canny::sobel_gradients(
        input.data(),
        Gx.data(),
        Gy.data(),
        width,
        height
    );

    // At pixel (row=3, col=3) which is right at the edge:
    // Gx should be large (strong horizontal change)
    // Gy should be small (no vertical change)
    // abs() because gradient can be positive or negative
    EXPECT_GT(abs(Gx[3 * width + 3]), abs(Gy[3 * width + 3]));
}

// TEST: HorizontalEdgeDetected
// --------------------------------
// IDEA: Create an image where the top half is black (0)
// and the bottom half is white (255). This creates a HORIZONTAL edge.
// The Sobel Gy kernel detects VERTICAL changes (top-bottom differences).
// So at the edge, Gy must be much larger than Gx.
TEST(SobelGradient, HorizontalEdgeDetected) {
    int width = 7, height = 7;

    // All pixels start as black (0)
    vector<uint8_t> input(width * height, 0);

    // Set bottom half (rows 4,5,6) to white (255)
    // This creates a horizontal edge between rows 3 and 4
    for (int r = 4; r < height; r++)
        for (int c = 0; c < width; c++)
            input[r * width + c] = 255;

    vector<int16_t> Gx(width * height, 0);
    vector<int16_t> Gy(width * height, 0);

    canny::sobel_gradients(
        input.data(),
        Gx.data(),
        Gy.data(),
        width,
        height
    );

    // At pixel (row=3, col=3) which is right at the edge:
    // Gy should be large (strong vertical change)
    // Gx should be small (no horizontal change)
    EXPECT_GT(abs(Gy[3 * width + 3]), abs(Gx[3 * width + 3]));
}
// diagonal edge → both Gx and Gy must be significant
TEST(SobelGradient, DiagonalEdgeDetected) {
    int width = 7, height = 7;

    vector<uint8_t> input(width * height, 0);
    for (int r = 0; r < height; r++)
        for (int c = 0; c < width; c++)
            if (c >= r) input[r * width + c] = 255;

    vector<int16_t> Gx(width * height, 0);
    vector<int16_t> Gy(width * height, 0);

    canny::sobel_gradients(
        input.data(),
        Gx.data(),
        Gy.data(),
        width,
        height
    );

    EXPECT_GT(abs(Gx[3 * width + 3]), 0);
    EXPECT_GT(abs(Gy[3 * width + 3]), 0);
}

// ================================================================
// MAGNITUDE TEST
// ================================================================

// TEST: L1AlwaysGreaterOrEqualL2
// --------------------------------
// IDEA: There are two ways to compute gradient magnitude:
//
//   L1 = |Gx| + |Gy|              → faster (no sqrt needed)
//   L2 = sqrt(Gx*Gx + Gy*Gy)     → more accurate
//
// Mathematically, L1 is ALWAYS >= L2.
// Proof: (|a| + |b|)² >= a² + b²  because 2|a||b| >= 0
//
// This test verifies:
//   1. Neither L1 nor L2 is zero (they actually detected something)
//   2. L1 >= L2 for every pixel (mathematical property holds)
TEST(Magnitude, L1AlwaysGreaterOrEqualL2) {
    int width = 5, height = 5;

    // Use fixed gradient values: Gx=100, Gy=100 for all pixels
    // So we can predict the exact expected results
    vector<int16_t> Gx(width * height, 100);
    vector<int16_t> Gy(width * height, 100);

    for (int i = 0; i < width * height; i++) {

        // L1 magnitude: sum of absolute values
        // |100| + |100| = 200
        int L1 = abs(Gx[i]) + abs(Gy[i]);

        // L2 magnitude: Euclidean distance (Pythagorean theorem)
        // sqrt(100² + 100²) = sqrt(20000) ≈ 141
        int L2 = (int)sqrt(
            (double)Gx[i] * Gx[i] + (double)Gy[i] * Gy[i]
        );

        EXPECT_GT(L1, 0);   // L1 must not be zero
        EXPECT_GT(L2, 0);   // L2 must not be zero
        EXPECT_GE(L1, L2);  // L1 (200) >= L2 (141) ✓
    }
}
