# RVV Gaussian — LMUL Experiment Results

## Overview

This experiment compares three LMUL configurations for the RVV Gaussian 5x5
convolution kernel, varying the LMUL used for the pixel load (and therefore
the widened intermediates and final accumulator, since the widening chain
uint8 -> int16 -> int32 doubles LMUL at each step):

| Configuration | Pixel load | Widened (int16) | Accumulator (int32) |
|----------------|------------|------------------|----------------------|
| LMUL=1         | `vuint8mf4_t`  | `vint16mf2_t` | `vint32m1_t` |
| LMUL=2         | `vuint8mf2_t`  | `vint16m1_t`  | `vint32m2_t` |
| LMUL=4         | `vuint8m1_t`   | `vint16m2_t`  | `vint32m4_t` |

A larger base LMUL processes more elements per vsetvl iteration, at the
cost of using more physical vector registers per logical variable.

## Methodology

- Image size: 256x256, interior pixels only (5x5 kernel, radius=2).
- Timing via clock() (bare-metal Newlib does not support
  clock_gettime/CLOCK_MONOTONIC).
- 100 iterations per configuration for stable measurement.
- QEMU user-mode is not cycle-accurate, so absolute timings are not
  meaningful — only relative comparisons (across LMUL, across VLEN) are
  valid, since they reflect actual instruction count differences.

## Results

| VLEN | LMUL=1 (sec/iter) | LMUL=2 (sec/iter) | LMUL=4 (sec/iter) |
|------|-------------------|-------------------|-------------------|
| 128  | 0.063127          | 0.033771          | 0.020792          |
| 256  | 0.035032          | 0.021063          | 0.015420          |
| 512  | 0.021328          | 0.015266          | 0.013641          |

### Relative speedup vs LMUL=1

| VLEN | LMUL=2 vs LMUL=1 | LMUL=4 vs LMUL=1 |
|------|------------------|------------------|
| 128  | ~1.87x faster    | ~3.04x faster    |
| 256  | ~1.66x faster    | ~2.27x faster    |
| 512  | ~1.40x faster    | ~1.56x faster    |

## Discussion

**Higher LMUL is faster at every VLEN tested, with no sign of regression.**
The Gaussian kernel uses very few distinct vector variables per kernel-loop
iteration (pixel load, two widening stages, multiply term, accumulator) —
even at LMUL=4 this stays well within the available register budget (8
logical registers at LMUL=4, vs 32 at LMUL=1). No register spilling was
observed at any tested configuration.

**The speedup from higher LMUL shrinks as VLEN grows.** At VLEN=128, going
from LMUL=1 to LMUL=4 processes roughly 4x the elements per vsetvl
iteration, which eliminates most of the per-iteration loop overhead.
At VLEN=512, each vsetvl call already processes a large chunk of the row
even at LMUL=1, so there is less relative overhead left — the gains compress
from ~3.0x down to ~1.6x.

**LMUL=1 is dramatically slower at small VLEN.** At VLEN=128, LMUL=1
processes very few elements per iteration. The fixed per-iteration overhead
dominates the useful work, explaining why LMUL=1 is ~3x slower than LMUL=4.

**Why we did not test LMUL=8:**
With 5 distinct vector variables alive simultaneously in the inner kernel
loop, LMUL=8 would very likely cause register spilling (only 4 logical
registers available), which would negate the speedup trend. Not tested due
to time constraints but is a natural follow-up.

## Conclusion

**LMUL=4 is the sweet spot** among the configurations tested. It consistently
outperforms both LMUL=1 and LMUL=2 across all VLEN values without triggering
register pressure issues. The benefit is most pronounced on smaller-VLEN
hardware (3x speedup over LMUL=1 at VLEN=128) — exactly the kind of embedded
target this project is designed for.
