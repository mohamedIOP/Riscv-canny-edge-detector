#include "../include/nms.hpp"

namespace canny {

// Non-Maximum Suppression.
// See nms.hpp for the direction → neighbour mapping and rationale.
void non_max_suppression(const uint8_t* mag, const uint8_t* dir,
                         uint8_t* out, size_t width, size_t height) {
    const int w = (int)width;
    const int h = (int)height;

    // Zero the whole output first — this also handles the 1-pixel border,
    // which we never write (it has no complete neighbourhood).
    for (size_t i = 0; i < width * height; i++) out[i] = 0;

    for (int y = 1; y < h - 1; y++) {
        for (int x = 1; x < w - 1; x++) {
            int idx = y * w + x;
            int m   = mag[idx];

            // A zero-magnitude pixel can never be an edge — skip the work.
            if (m == 0) continue;

            int n1, n2;  // the two neighbours along the gradient direction
            switch (dir[idx]) {
                case 0:  // horizontal gradient → look left / right
                    n1 = mag[idx - 1];
                    n2 = mag[idx + 1];
                    break;
                case 1:  // main diagonal (45°) → up-left / down-right
                    n1 = mag[idx - w - 1];
                    n2 = mag[idx + w + 1];
                    break;
                case 2:  // vertical gradient → up / down
                    n1 = mag[idx - w];
                    n2 = mag[idx + w];
                    break;
                default: // case 3: anti-diagonal (135°) → up-right / down-left
                    n1 = mag[idx - w + 1];
                    n2 = mag[idx + w - 1];
                    break;
            }

            // Keep the pixel only if it is a local maximum across the edge.
            // Using >= on both sides keeps ridge plateaus stable and avoids
            // dropping a genuine maximum that happens to tie a neighbour.
            if (m >= n1 && m >= n2)
                out[idx] = (uint8_t)m;
            // else: leave out[idx] = 0 (suppressed)
        }
    }
}

}  // namespace canny
