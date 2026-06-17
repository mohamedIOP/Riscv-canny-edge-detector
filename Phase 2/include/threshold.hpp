#ifndef THRESHOLD_HPP
#define THRESHOLD_HPP

#include <cstdint>
#include <cstddef>

namespace canny {

// Pixel labels produced by double_threshold() and consumed by hysteresis().
// Chosen so the intermediate buffer is also directly viewable as a raw image
// (0 = black, 128 = grey weak, 255 = white strong).
constexpr uint8_t EDGE_NONE   = 0;
constexpr uint8_t EDGE_WEAK   = 128;
constexpr uint8_t EDGE_STRONG = 255;

// ── Stage 4: Double Thresholding ─────────────────────────────────────────
// Classifies each pixel of the (thinned) magnitude image into three classes:
//
//   mag >= high            → EDGE_STRONG (255)   definite edge
//   low  <= mag < high     → EDGE_WEAK   (128)   candidate, needs hysteresis
//   mag <  low             → EDGE_NONE   (0)     suppressed
//
// `low` and `high` are absolute thresholds on the [0,255] normalized
// magnitude. A common heuristic is high ≈ 2–3× low; the defaults below work
// well on the project's normalized magnitude. Requires high >= low.
void double_threshold(const uint8_t* mag, uint8_t* out,
                      size_t width, size_t height,
                      uint8_t low_thresh = 20, uint8_t high_thresh = 50);

// ── Stage 5: Hysteresis Edge Tracing ─────────────────────────────────────
// Promotes EDGE_WEAK pixels that are 8-connected (directly or transitively)
// to an EDGE_STRONG pixel into final edges; discards all other weak pixels.
//
// Implemented as an explicit stack-based flood fill seeded from every strong
// pixel (no recursion — safe on the bare-metal QEMU target, and O(N)).
// Output is a binary image: 255 (edge) or 0 (background).
//
// `in` and `out` may alias the same buffer (the implementation copies first).
void hysteresis(const uint8_t* in, uint8_t* out,
                size_t width, size_t height);

}  // namespace canny

#endif
