# RVV Gaussian — LMUL Experiment Results

## Overview

This experiment compares two LMUL configurations for the RVV Gaussian 5x5
convolution kernel:

- **LMUL=2**: pixels loaded as `vuint8mf2_t`, widened through `vuint16m1_t` /
  `vint16m1_t`, accumulated in `vint32m2_t`.
- **LMUL=4**: pixels loaded as `vuint8m1_t`, widened through `vuint16m2_t` /
  `vint16m2_t`, accumulated in `vint32m4_t`.

The widening chain doubles LMUL at each step (8-bit -> 16-bit -> 32-bit), so
choosing a larger base LMUL for the pixel load processes more elements per
`vsetvl` iteration, at the cost of using more physical vector registers per
logical variable.

## Methodology

- Image size: 256x256, interior pixels only (5x5 kernel, radius=2).
- Timing via `clock()` (bare-metal Newlib does not support
  `clock_gettime`/`CLOCK_MONOTONIC`).
- 100 iterations per configuration for stable measurement.
- QEMU user-mode is not cycle-accurate, so absolute timings are not
  meaningful — only relative comparisons (LMUL=2 vs LMUL=4, across VLEN)
  are valid, since they reflect actual instruction count differences.

## Results

| VLEN | LMUL=2 (sec/iter) | LMUL=4 (sec/iter) | Speedup (LMUL=4 vs LMUL=2) |
|------|-------------------|-------------------|----------------------------|
| 128  | 0.034142          | 0.021756          | ~36% faster                |
| 256  | 0.021181          | 0.015424          | ~27% faster                |
| 512  | 0.015746          | 0.013631          | ~13% faster                |

## Discussion

**LMUL=4 is faster at every VLEN tested.** The Gaussian kernel uses very few
distinct vector variables (pixel load, widened intermediates, accumulator,
result) — well within the register budget even at LMUL=4 (8 logical
registers available). No register spilling was observed, so the larger LMUL
simply means fewer `vsetvl` iterations and less per-iteration loop overhead
(pointer arithmetic, branch, vl computation) for the same total amount of
work.

**The speedup shrinks as VLEN grows.** At VLEN=128, LMUL=4 processes roughly
twice the elements per iteration compared to LMUL=2, which significantly
reduces loop overhead relative to useful work. At VLEN=512, each `vsetvl`
call already processes a large chunk of the row, so the *relative* overhead
of extra iterations at LMUL=2 is smaller to begin with — there is less
overhead left to eliminate by going to LMUL=4.

**Why we did not test LMUL=1 or LMUL=8:**
- LMUL=1 for the pixel load would require an LMUL=2 accumulator after the
  uint8->int32 widening chain (matching our original "LMUL=2" config — this
  is actually the baseline, not a separate lower point).
- LMUL=8 for the int32 accumulator is the architectural maximum and was not
  tested due to time constraints, but is expected to show diminishing or
  negative returns once the 8 kernel-loop temporaries (pixel load, two
  widening stages, multiply term, accumulator) begin to compete for the 4
  logical registers available at LMUL=8 — likely causing spills.

## Conclusion

For this kernel's register usage profile, **LMUL=4 is the sweet spot** among
the configurations tested. It consistently outperforms LMUL=2 across all
VLEN values without triggering register pressure issues, and the gain is
most pronounced on smaller VLEN hardware where loop overhead is a larger
fraction of total work.
