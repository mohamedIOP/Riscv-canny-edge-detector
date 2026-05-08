#include "pipeline.hpp"

// ============================================================
// convolve2D — Generic 2D Convolution Function
// ============================================================
// This is a TEMPLATE function, meaning it works with any data type.
// PixelType = the image pixel type       → we use uint8_t  (values 0-255)
// AccumType = the math accumulator type  → we use int32_t  (big enough to avoid overflow)
//
// What does convolution mean?
// For every pixel in the image, we look at its neighbors,
// multiply each neighbor by a weight from the kernel,
// then sum everything up to get the new pixel value.
// ============================================================

template <typename PixelType, typename AccumType>
void convolve2D(
    const PixelType* input,    // original image  (read only)
    PixelType*       output,   // result image    (we write here)
    int              width,    // image width  in pixels
    int              height,   // image height in pixels
    const float*     kernel,   // the weight matrix (e.g. 5x5 = 25 numbers)
    int              kernelSize // size of one side  (5 for 5x5, 3 for 3x3)
)
{
    // radius = how many pixels to look around the center
    // 5x5 kernel → radius = 2  (2 pixels left, right, up, down)
    // 3x3 kernel → radius = 1  (1 pixel  left, right, up, down)
    int radius = kernelSize / 2;

    // ── Outer loops: visit every pixel in the image ──────────────
    for (int y = 0; y < height; y++) {        // row by row
        for (int x = 0; x < width; x++) {     // column by column

            // sum will accumulate the weighted values of all neighbors
            // We use AccumType (int32_t) here, NOT uint8_t,
            // because the sum can exceed 255 during calculation
            AccumType sum = 0;

            // ── Inner loops: slide the kernel over neighbors ──────
            // ky and kx go from -radius to +radius
            // Example for radius=2: -2, -1, 0, +1, +2
            for (int ky = -radius; ky <= radius; ky++) {
                for (int kx = -radius; kx <= radius; kx++) {

                    // iy, ix = the actual position of the neighbor pixel
                    int iy = y + ky;
                    int ix = x + kx;

                    // ZERO-PADDING:
                    // If the neighbor is outside the image boundaries,
                    // treat it as 0 (black pixel) instead of crashing.
                    // This is why border pixels come out slightly darker.
                    PixelType pixel = 0;
                    if (ix >= 0 && ix < width && iy >= 0 && iy < height)
                        pixel = input[iy * width + ix];
                    //        └─────────────────────┘
                    //        convert 2D position (iy, ix)
                    //        to 1D array index

                    // k_idx = position of the matching weight in the kernel array
                    // kernel is stored as a flat 1D array, row by row
                    // Example: kernel[0][0] is at index 0
                    //          kernel[1][2] is at index (1*5 + 2) = 7
                    int k_idx = (ky + radius) * kernelSize + (kx + radius);

                    // Multiply pixel value by its kernel weight and add to sum
                    sum += (AccumType)pixel * (AccumType)kernel[k_idx];
                }
            }

            // CLAMPING:
            // After all the multiplication and addition,
            // the result might be outside [0, 255].
            // We force it back into the valid pixel range.
            if (sum < 0)   sum = 0;    // no negative pixels
            if (sum > 255) sum = 255;  // no pixels brighter than 255

            // Write the final result to the output image
            // Convert back from AccumType (int32_t) to PixelType (uint8_t)
            output[y * width + x] = (PixelType)sum;
        }
    }
}

// ============================================================
// Explicit Template Instantiation
// ============================================================
// C++ templates are "lazy" — the compiler only generates code
// when it sees the template being used.
// Because convolve2D is defined in .cpp (not .hpp),
// the linker won't find it unless we explicitly tell the compiler
// "generate this specific version right here."
//
// This line generates the version where:
//   PixelType = uint8_t  (grayscale pixel, 0-255)
//   AccumType = int32_t  (32-bit signed integer for safe math)
// ============================================================
template void convolve2D<uint8_t, int32_t>(
    const uint8_t*, uint8_t*, int, int, const float*, int);
