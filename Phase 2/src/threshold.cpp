#include "../include/threshold.hpp"
#include <cstdlib>   // malloc / free

namespace canny {

// ── Stage 4: Double Thresholding ─────────────────────────────────────────
void double_threshold(const uint8_t* mag, uint8_t* out,
                      size_t width, size_t height,
                      uint8_t low_thresh, uint8_t high_thresh) {
    size_t total = width * height;
    for (size_t i = 0; i < total; i++) {
        uint8_t m = mag[i];
        if (m >= high_thresh)      out[i] = EDGE_STRONG;
        else if (m >= low_thresh)  out[i] = EDGE_WEAK;
        else                       out[i] = EDGE_NONE;
    }
}

// ── Stage 5: Hysteresis Edge Tracing ─────────────────────────────────────
// Explicit-stack flood fill. Every strong pixel is a seed; from a seed we
// walk to any 8-connected weak pixel, mark it as a confirmed edge, and push
// it so its own weak neighbours are visited too. Weak pixels never reached
// from a strong seed are dropped at the end.
void hysteresis(const uint8_t* in, uint8_t* out,
                size_t width, size_t height) {
    const int w = (int)width;
    const int h = (int)height;
    const size_t total = width * height;

    // Work on a private copy so callers may pass in == out.
    // 'state' holds the tri-level labels while we trace; we overwrite it
    // in place as weak pixels get promoted to strong.
    uint8_t* state = (uint8_t*)malloc(total);
    if (!state) return;
    for (size_t i = 0; i < total; i++) state[i] = in[i];

    // LIFO stack of pixel indices waiting to have their neighbours scanned.
    // Worst case every pixel is pushed exactly once → total entries suffice.
    int* stack = (int*)malloc(total * sizeof(int));
    if (!stack) { free(state); return; }
    size_t sp = 0;  // stack pointer (number of items on the stack)

    // Seed the stack with all strong pixels.
    for (int i = 0; i < (int)total; i++)
        if (state[i] == EDGE_STRONG)
            stack[sp++] = i;

    // 8-connectivity neighbour offsets.
    static const int DX[8] = { -1, 0, 1, -1, 1, -1, 0, 1 };
    static const int DY[8] = { -1, -1, -1, 0, 0, 1, 1, 1 };

    while (sp > 0) {
        int idx = stack[--sp];
        int y = idx / w;
        int x = idx - y * w;

        for (int k = 0; k < 8; k++) {
            int nx = x + DX[k];
            int ny = y + DY[k];
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;

            int nidx = ny * w + nx;
            if (state[nidx] == EDGE_WEAK) {
                // Connected to a strong chain → promote and keep tracing.
                state[nidx] = EDGE_STRONG;
                stack[sp++] = nidx;
            }
        }
    }

    // Finalize: confirmed edges → 255, everything else (including leftover
    // un-promoted weak pixels) → 0.
    for (size_t i = 0; i < total; i++)
        out[i] = (state[i] == EDGE_STRONG) ? 255 : 0;

    free(stack);
    free(state);
}

}  // namespace canny
