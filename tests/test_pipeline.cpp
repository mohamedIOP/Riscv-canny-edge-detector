#include <gtest/gtest.h>
#include "../src/pipeline.hpp"
#include "../Phase 2/include/gaussian.hpp"
#include "../Phase 2/include/magnitude.hpp"
#include "../Phase 2/include/direction.hpp"
#include "../Phase 2/include/sobel.hpp"
#include "../Phase 2/include/nms.hpp"
#include "../Phase 2/include/threshold.hpp"
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
// NON-MAXIMUM SUPPRESSION TESTS (BONUS — Stage 3)
// ================================================================
// TEST: BorderIsZeroed
// --------------------------------
// IDEA: NMS needs a full neighbourhood, so the 1-pixel border can
// never be evaluated. The implementation must leave it at 0.
TEST(NMS, BorderIsZeroed) {
size_t W = 8, H = 8;
    vector<uint8_t> mag(W * H, 200);   // strong everywhere
    vector<uint8_t> dir(W * H, 0);
    vector<uint8_t> out(W * H, 123);   // garbage to prove it gets overwritten

    canny::non_max_suppression(mag.data(), dir.data(), out.data(), W, H);

    for (size_t x = 0; x < W; x++) {
        EXPECT_EQ(out[x], 0);                 // top row
        EXPECT_EQ(out[(H - 1) * W + x], 0);   // bottom row
    }
    for (size_t y = 0; y < H; y++) {
        EXPECT_EQ(out[y * W], 0);             // left col
        EXPECT_EQ(out[y * W + (W - 1)], 0);   // right col
    }
}
// TEST: KeepsHorizontalRidgePeak
// --------------------------------
// IDEA: With a horizontal magnitude ramp 10,20,30,20,10 and direction 0
// (gradient horizontal → compare left/right), only the central peak (30)
// is a local maximum. The flanking 20s must be suppressed to 0.
TEST(NMS, KeepsHorizontalRidgePeak) {
    size_t W = 5, H = 3;
    vector<uint8_t> mag(W * H, 0);
    vector<uint8_t> dir(W * H, 0);   // all horizontal gradient
    vector<uint8_t> out(W * H, 0);

    // Middle row: 10 20 30 20 10
    uint8_t row[5] = {10, 20, 30, 20, 10};
    for (size_t x = 0; x < W; x++) mag[1 * W + x] = row[x];

    canny::non_max_suppression(mag.data(), dir.data(), out.data(), W, H);

    EXPECT_EQ(out[1 * W + 2], 30);  // peak kept
    EXPECT_EQ(out[1 * W + 1], 0);   // left shoulder suppressed
    EXPECT_EQ(out[1 * W + 3], 0);   // right shoulder suppressed
}

// TEST: SinglePixelSurvives
// --------------------------------
// IDEA: One bright pixel on a black field is >= all its (zero) neighbours
// in every direction, so NMS must keep it unchanged.
TEST(NMS, SinglePixelSurvives) {
    size_t W = 5, H = 5;
    vector<uint8_t> mag(W * H, 0);
    vector<uint8_t> dir(W * H, 2);   // vertical gradient (arbitrary)
    vector<uint8_t> out(W * H, 0);

    mag[2 * W + 2] = 180;
    canny::non_max_suppression(mag.data(), dir.data(), out.data(), W, H);

    EXPECT_EQ(out[2 * W + 2], 180);
}

// ================================================================
// DOUBLE THRESHOLD TESTS (BONUS — Stage 4)
// ================================================================

// TEST: ThreeClassesAssigned
// --------------------------------
// IDEA: With low=20, high=50, values below 20 → 0, [20,50) → weak (128),
// >=50 → strong (255).
TEST(Threshold, ThreeClassesAssigned) {
    size_t W = 3, H = 1;
    vector<uint8_t> in  = {10, 30, 60};
    vector<uint8_t> out(W * H, 99);

    canny::double_threshold(in.data(), out.data(), W, H, 20, 50);

    EXPECT_EQ(out[0], canny::EDGE_NONE);    // 10  < low
    EXPECT_EQ(out[1], canny::EDGE_WEAK);    // 20 <= 30 < 50
    EXPECT_EQ(out[2], canny::EDGE_STRONG);  // 60 >= high
}

// TEST: BoundaryValuesInclusive
// --------------------------------
// IDEA: A value exactly equal to a threshold takes the higher class
// (>= semantics): low itself is weak, high itself is strong.
TEST(Threshold, BoundaryValuesInclusive) {
    size_t W = 2, H = 1;
    vector<uint8_t> in  = {20, 50};
    vector<uint8_t> out(W * H, 0);

    canny::double_threshold(in.data(), out.data(), W, H, 20, 50);

    EXPECT_EQ(out[0], canny::EDGE_WEAK);    // == low  → weak
    EXPECT_EQ(out[1], canny::EDGE_STRONG);  // == high → strong
}

// ================================================================
// HYSTERESIS TESTS (BONUS — Stage 5)
// ================================================================

// TEST: WeakChainConnectedToStrongIsKept
// --------------------------------
// IDEA: A run [STRONG, WEAK, WEAK, NONE, WEAK]. The two weak pixels touching
// the strong one are promoted to edges; the isolated weak pixel after the
// gap is discarded.
TEST(Hysteresis, WeakChainConnectedToStrongIsKept) {
    size_t W = 5, H = 1;
    using namespace canny;
    vector<uint8_t> in = {EDGE_STRONG, EDGE_WEAK, EDGE_WEAK, EDGE_NONE, EDGE_WEAK};
    vector<uint8_t> out(W * H, 0);

    hysteresis(in.data(), out.data(), W, H);

    EXPECT_EQ(out[0], 255);  // strong
    EXPECT_EQ(out[1], 255);  // weak, connected
    EXPECT_EQ(out[2], 255);  // weak, connected through pixel 1
    EXPECT_EQ(out[3], 0);    // none
    EXPECT_EQ(out[4], 0);    // weak, isolated → dropped
}

// TEST: IsolatedWeakIsDropped
// --------------------------------
// IDEA: Weak pixels with no strong pixel anywhere must all vanish.
TEST(Hysteresis, IsolatedWeakIsDropped) {
    size_t W = 4, H = 4;
    vector<uint8_t> in(W * H, canny::EDGE_WEAK);
    vector<uint8_t> out(W * H, 0);

    canny::hysteresis(in.data(), out.data(), W, H);

    for (size_t i = 0; i < W * H; i++)
        EXPECT_EQ(out[i], 0);
}

// TEST: DiagonalConnectivity
// --------------------------------
// IDEA: Hysteresis uses 8-connectivity, so a weak pixel touching a strong
// one only diagonally must still be promoted.
TEST(Hysteresis, DiagonalConnectivity) {
    size_t W = 3, H = 3;
    using namespace canny;
    vector<uint8_t> in(W * H, EDGE_NONE);
    in[0 * W + 0] = EDGE_STRONG;   // top-left
    in[1 * W + 1] = EDGE_WEAK;     // centre, diagonal neighbour
    vector<uint8_t> out(W * H, 0);
    hysteresis(in.data(), out.data(), W, H);

    EXPECT_EQ(out[0 * W + 0], 255);
    EXPECT_EQ(out[1 * W + 1], 255);  // promoted via diagonal
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
// ================================================================
// MAGNITUDE_L1 FUNCTION TESTS
// ================================================================

TEST(Magnitude, L1OutputNonZeroOnEdge) {
    size_t width = 5, height = 5;
    vector<int16_t> Gx(width * height, 100);
    vector<int16_t> Gy(width * height, 100);
    vector<uint8_t> mag(width * height, 0);

    canny::magnitude_l1(
        Gx.data(), Gy.data(), mag.data(), width, height
    );

    // gradients are non-zero → magnitude must be non-zero
    for (size_t i = 0; i < width * height; i++) {
        EXPECT_GT(mag[i], 0);
    }
}

TEST(Magnitude, L1ZeroOnFlatImage) {
    size_t width = 5, height = 5;
    vector<int16_t> Gx(width * height, 0);
    vector<int16_t> Gy(width * height, 0);
    vector<uint8_t> mag(width * height, 0);

    canny::magnitude_l1(
        Gx.data(), Gy.data(), mag.data(), width, height
    );

    // all gradients zero → magnitude must be zero
    for (size_t i = 0; i < width * height; i++) {
        EXPECT_EQ(mag[i], 0);
    }
}

// ================================================================
// GRADIENT DIRECTION TESTS
// ================================================================

TEST(Direction, VerticalEdgeIsDirection0) {
    size_t width = 7, height = 7;
    // large Gx, zero Gy → horizontal gradient → direction 0
    vector<int16_t> Gx(width * height, 500);
    vector<int16_t> Gy(width * height, 0);
    vector<uint8_t> dir(width * height, 0);

    canny::gradient_direction(
        Gx.data(), Gy.data(), dir.data(), width, height
    );

    for (size_t i = 0; i < width * height; i++) {
        EXPECT_EQ(dir[i], 0);
    }
}

TEST(Direction, HorizontalEdgeIsDirection2) {
    size_t width = 7, height = 7;
    // zero Gx, large Gy → vertical gradient → direction 2
    vector<int16_t> Gx(width * height, 0);
    vector<int16_t> Gy(width * height, 500);
    vector<uint8_t> dir(width * height, 0);

    canny::gradient_direction(
        Gx.data(), Gy.data(), dir.data(), width, height
    );

    for (size_t i = 0; i < width * height; i++) {
        EXPECT_EQ(dir[i], 2);
    }
}

TEST(Direction, DiagonalIsDirection1or3) {
    size_t width = 7, height = 7;
    // equal Gx and Gy → diagonal → direction 1 or 3
    vector<int16_t> Gx(width * height, 300);
    vector<int16_t> Gy(width * height, 300);
    vector<uint8_t> dir(width * height, 0);

    canny::gradient_direction(
        Gx.data(), Gy.data(), dir.data(), width, height
    );

    for (size_t i = 0; i < width * height; i++) {
        EXPECT_TRUE(dir[i] == 1 || dir[i] == 3);
    }
}
