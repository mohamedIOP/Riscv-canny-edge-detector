# Optimization Results

All measurements taken on QEMU user-mode emulation (not cycle-accurate).
Image: 256×256 grayscale. Iterations: 100. VLEN=256 for flag sweep.

---

## Compiler Flag Sweep

### Total Pipeline Time (ms)

| Flag   | Gaussian | Sobel  | Mag L1 | Mag L2  | Direction | **Total** |
|--------|----------|--------|--------|---------|-----------|-----------|
| -O0    | 20.79    | 15.13  | 4.99   | 35.80   | 2.82      | **79.52** |
| -O2    | 6.49     | 3.71   | 3.38   | 11.37   | 0.36      | **25.32** |
| -O3    | 3.82     | 1.13   | 7.05   | 11.08   | 5.63      | **28.71** |
| -Os    | 6.87     | 2.86   | 0.53   | 33.01   | 0.44      | **43.70** |
| -Ofast | 13.02    | 1.12   | 6.96   | 14.00   | 5.60      | **40.70** |

### Binary Sizes

| Flag   | Size (KB) |
|--------|-----------|
| -O0    | 120.7     |
| -O2    | 116.8     |
| -O3    | 118.8     |
| -Os    | 117.2     |
| -Ofast | 119.5     |

### Key Observations
- **-O2 is the sweet spot** — fastest at 25.32ms (3.1× speedup over -O0)
- **-O3 is slower than -O2** (28.71ms) — aggressive inlining increases instruction cache pressure on QEMU
- **Sobel biggest winner** — 15.13ms at -O0 → 1.13ms at -O3 (13× compiler speedup)
- **Binary sizes nearly identical** (116–121 KB) — static runtime dominates, pipeline code is small
- **-Os trades speed for size** — slowest optimized flag at 43.70ms but smallest binary

---

## Profiling Baseline (-O3, scalar)

### Per-VLEN Breakdown

| Stage        | VLEN=128 | % Time | VLEN=256 | % Time | VLEN=512 | % Time |
|--------------|----------|--------|----------|--------|----------|--------|
| Gaussian 5×5 | 3.86 ms  | 21.9%  | 3.85 ms  | 13.3%  | 3.92 ms  | 16.1%  |
| Sobel Gx/Gy  | 1.14 ms  | 6.5%   | 1.14 ms  | 3.9%   | 1.15 ms  | 4.7%   |
| Magnitude L1 | 4.81 ms  | 27.3%  | 7.14 ms  | 24.6%  | 4.42 ms  | 18.1%  |
| Magnitude L2 | 4.22 ms  | 24.0%  | 11.24 ms | 38.7%  | 11.41 ms | 46.8%  |
| Direction    | 3.59 ms  | 20.4%  | 5.69 ms  | 19.6%  | 3.47 ms  | 14.3%  |
| **TOTAL**    | **17.62 ms** |    | **29.05 ms** |    | **24.37 ms** |    |

### RVV Optimization Priority (Amdahl's Law)
1. **Magnitude L2** — 24–47% of time → highest impact
2. **Gaussian 5×5** — 13–22% of time → second priority
3. **Direction** — 14–20% of time → third priority
4. **Sobel Gx/Gy** — only 4–7% → skip, negligible gain

---

## Auto-Vectorization Analysis
*(to be filled after running -fopt-info-vec-all)*

| Stage        | Auto-vectorized? | Reason if not |
|--------------|-----------------|---------------|
| Gaussian 5×5 | TBD             |               |
| Sobel Gx/Gy  | TBD             |               |
| Magnitude L1 | TBD             |               |
| Magnitude L2 | TBD             |               |
| Direction    | TBD             |               |

---

## RVV Intrinsic Results
*(to be filled after Student C implements RVV)*

| Stage        | Scalar -O3 | RVV VLEN=128 | RVV VLEN=256 | RVV VLEN=512 | Speedup |
|--------------|------------|--------------|--------------|--------------|---------|
| Gaussian 5×5 | 3.85 ms    | TBD          | TBD          | TBD          | TBD     |
| Magnitude L2 | 11.24 ms   | TBD          | TBD          | TBD          | TBD     |
| **TOTAL**    | 29.05 ms   | TBD          | TBD          | TBD          | TBD     |