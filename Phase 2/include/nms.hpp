#ifndef NMS_HPP
#define NMS_HPP

#include <cstdint>
#include <cstddef>

namespace canny {

// ── Stage 3: Non-Maximum Suppression (NMS) ───────────────────────────────
// Thins the gradient-magnitude image to single-pixel-wide ridges.
//
// For every interior pixel we look at the two neighbours that lie along the
// quantized gradient direction (dir ∈ {0,1,2,3} from gradient_direction()):
//
//   dir 0  (0°,  horizontal gradient) → compare (x-1) and (x+1)
//   dir 1  (45°, main diagonal)       → compare (x-1,y-1) and (x+1,y+1)
//   dir 2  (90°, vertical gradient)   → compare (x,y-1) and (x,y+1)
//   dir 3  (135°, anti-diagonal)      → compare (x+1,y-1) and (x-1,y+1)
//
// A pixel is kept only if its magnitude is >= BOTH of those neighbours
// (i.e. it is a local maximum across the edge). Otherwise it is suppressed
// to 0. The 1-pixel border is set to 0 (no full neighbourhood available).
//
// Inputs are the normalized magnitude (uint8, L1 or L2) and the quantized
// direction. Output is a thinned uint8 magnitude image.
//
// This stage is inherently data-dependent (neighbour choice branches on the
// direction), so it stays scalar — the profiling philosophy of the project
// (only vectorize hot, branch-free kernels) applies here.
void non_max_suppression(const uint8_t* mag, const uint8_t* dir,
                         uint8_t* out, size_t width, size_t height);

}  // namespace canny

#endif
