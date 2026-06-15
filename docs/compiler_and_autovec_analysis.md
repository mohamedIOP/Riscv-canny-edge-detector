# Compiler Optimization & Auto-Vectorization Analysis

**Author:** Student B · **Scope:** Phase 4 (flag sweep) + auto-vectorization investigation
**Inputs:** raw timing data in `optimization_results.md` (Student A), compiler output in `vec_report.txt`
**Target:** `rv64gcv`, GCC `-march=rv64gcv -mabi=lp64d`, QEMU user-mode, 256×256 image, 100 iterations

> This document is the written analysis that accompanies the raw numbers in
> `optimization_results.md`. It interprets *why* the measurements look the way
> they do and connects the auto-vectorization findings to the motivation for
> hand-written RVV intrinsics.

---

## 1. Measurement Caveat (read first)

QEMU user-mode is **not cycle-accurate** — it translates RISC-V instructions to
host instructions and does not model caches, pipelines, or memory latency. The
absolute millisecond figures are therefore meaningless as hardware estimates.
What *is* valid is the **relative comparison** between builds of the same code,
because the translated instruction count changes with optimization level and
with scalar-vs-vector codegen. Every claim below is a relative one (O0 vs O2,
auto-vec vs scalar), measured with `clock_gettime(CLOCK_MONOTONIC)` averaged
over 100 iterations for stability. This limitation is restated in the final
report as required.

---

## 2. Compiler Flag Sweep Analysis

Total pipeline time (ms), from `optimization_results.md`:

| Flag   | Total ms | vs -O0 | Binary KB |
|--------|---------:|-------:|----------:|
| -O0    | 79.52    | 1.0×   | 120.7 |
| **-O2**| **25.32**| **3.1×**| 116.8 |
| -O3    | 28.71    | 2.8×   | 118.8 |
| -Os    | 43.70    | 1.8×   | 117.2 |
| -Ofast | 40.70    | 2.0×   | 119.5 |

### 2.1 The headline result: -O2 is the sweet spot, -O3 is *slower*

This is the most important and least intuitive finding, so it gets the most
discussion. `-O2` is the fastest build at **25.32 ms**, and `-O3` is **13.4%
slower** despite being a "higher" optimization level. The reason is that the
project's hot loops (Gaussian, Sobel) **cannot be auto-vectorized** (Section 3),
so `-O3`'s extra passes give no SIMD payoff on them. Instead `-O3` applies
aggressive inlining and loop unrolling that *inflate* the translated code
QEMU has to execute, with no compensating data-parallel speedup. On a
cycle-accurate machine the larger code might still win via better ILP; under
QEMU's instruction-count-dominated model it loses. The lesson — *a higher `-O`
level is not automatically faster* — is exactly the kind of data-driven result
the project asks us to surface rather than assume.

### 2.2 Per-stage behavior (where the speedup actually comes from)

| Stage     | -O0   | -O2   | -O3   | Note |
|-----------|------:|------:|------:|------|
| Gaussian  | 20.79 | 6.49  | 3.82  | scalar; gains are pure scalar codegen (strength reduction, register allocation) |
| Sobel     | 15.13 | 3.71  | 1.13  | **13× O0→O3** — biggest compiler win; tight inner loop unrolls extremely well |
| Mag L1    |  4.99 | 3.38  | 7.05  | auto-vectorized — but `-O3` adds an aliasing-versioning branch (§3.3) |
| Mag L2    | 35.80 | 11.37 | 11.08 | dominated by scalar `double sqrt`; never vectorizes; flat O2→O3 |
| Direction |  2.82 | 0.36  | 5.63  | cheap; `-O3` codegen regresses it under QEMU |

Two stages account for the `-O3` regression: **Mag L1** (3.38→7.05) and
**Direction** (0.36→5.63). Both are auto-vectorized loops, and in both cases
`-O3`'s versioning/unrolling added overhead that outweighed the benefit on a
256×256 image. Sobel is the clearest "free lunch": the compiler shrinks it 13×
without any source changes, simply by unrolling the fixed 3×3 kernel.

### 2.3 The -Os and -Ofast trade-offs

- **-Os** optimizes for size and is the slowest *optimized* build (43.70 ms).
  It produces the second-smallest binary but suppresses the unrolling that
  helps Sobel/Gaussian, so speed suffers. Useful data point for the
  size-vs-speed discussion, but not a build we'd ship for this workload.
- **-Ofast** (40.70 ms) enables `-ffast-math`, which **changes** the L2 result
  numerically (reassociated/relaxed FP `sqrt`). It is *not* faster here and it
  breaks bit-exact equivalence with the scalar baseline, so it is unsuitable as
  the reference build for the RVV equivalence tests. Avoid for correctness work.

### 2.4 Why binary sizes barely move (116–121 KB)

All five binaries sit within a 4 KB band. The statically-linked Newlib runtime
dominates the image; the pipeline's own code is a few KB. So binary size is a
poor discriminator here and we lean on timing instead. Worth one sentence in
the report, no more.

**Recommendation:** build the reference/measurement pipeline at **-O2**. It is
fastest, keeps bit-exact FP (unlike `-Ofast`), and is the honest scalar baseline
the RVV speedups should be measured against.

---

## 3. Auto-Vectorization Investigation (`-O3 -fopt-info-vec-all`)

Method: compile with `-O3 -fopt-info-vec-all`, capture to `vec_report.txt`,
and count emitted vector instructions with
`riscv64-unknown-elf-objdump -d canny_vec_report | grep -c vset` → **129 `vset`
instructions**. The table below maps every stage to the *actual* compiler
message (file:line from `vec_report.txt`).

| Stage | Result | Compiler evidence (`vec_report.txt`) |
|-------|--------|--------------------------------------|
| Gaussian 5×5 | ❌ scalar | `convolution.hpp:28` *unsupported control flow in loop*; `gaussian.cpp:14` *vectorized 0 loops* |
| Sobel Gx/Gy | ❌ scalar | `sobel.cpp:22,23,27` *unsupported control flow in loop*; `sobel.cpp:17` *vectorized 0 loops* |
| Magnitude L1 | ✅ both passes | `magnitude.cpp:16` & `:28` *loop vectorized using variable length vectors*; `:10` *vectorized 2 loops* |
| Magnitude L2 | ❌ scalar | `main.cpp:156` *unsupported data-type* (the `double sqrt` loop) |
| Direction | ✅ main loop | `direction.cpp:13` *loop vectorized using variable length vectors*; `:9` *vectorized 1 loops* |
| Gx/Gy visualization | ✅ both | `main.cpp:183,193` *loop vectorized using variable length vectors* |

### 3.1 The root cause: the boundary check blocks Gaussian and Sobel

The single most important finding. The convolution inner loop contains:

```cpp
// convolution.hpp:28  (and the equivalent in sobel.cpp:22/23/27)
if (ix >= 0 && ix < width && iy >= 0 && iy < height)
    pixel = input[iy * width + ix];
```

GCC reports this verbatim as **`not vectorized: unsupported control flow in
loop`**. The data-dependent branch on the loop index prevents the vectorizer
from proving a uniform access pattern, so it gives up — `gaussian.cpp:14` and
`sobel.cpp:17` both report *"vectorized 0 loops in function."* The compiler
genuinely tries: `vec_report.txt` shows it re-running analysis across every
RVV vector mode (`RVVM1QI`, `RVVMF2QI`, …) and failing each time.

There is a secondary blocker in the template too: the nested kernel loops
(`convolution.hpp:24/25`) trip *"loop nest containing two or more consecutive
inner loops cannot be vectorized"*, and the normalize/store loop
(`convolution.hpp:38`) is rejected as *"not vectorized: vectorization is not
profitable."* But the boundary `if` is the headline cause and the one we remove
in the RVV rewrite.

### 3.2 Why L2 magnitude never vectorizes

The L2 loop computes `sqrt((double)gx*gx + (double)gy*gy)`. GCC reports
**`not vectorized: unsupported data-type`** at `main.cpp:156`. The `double`
`sqrt` is the problem: GCC's RVV auto-vectorizer will not emit a vector
floating-point sqrt for this loop shape, so all of L2 stays scalar. This
explains why L2 is the single most expensive stage (11+ ms at every optimized
level) and why it is the **#1 RVV target** under Amdahl's law — it is both the
hottest stage and the one the compiler does nothing for.

### 3.3 What the compiler *did* vectorize — and a subtlety

- **Magnitude L1** vectorizes **both** passes (max-find at `:16`, normalize at
  `:28`) using variable-length vectors. This is why L1 is comparatively cheap.
  Note the message *"loop versioned for vectorization because of possible
  aliasing"* on the normalize pass: because `gx`, `gy`, `mag_out` are plain
  pointers that could overlap, GCC emits a **runtime aliasing check** and two
  loop versions (vector + scalar fallback). That versioning branch is part of
  why Mag L1 *regressed* at `-O3` in the timing table (§2.2). Adding
  `__restrict__` to the pointers would let the compiler drop the check.
- **Direction** vectorizes its main loop (`:13`). The branchy 0/45/90/135
  quantization is handled with vector compares + masked blends rather than
  scalar control flow — a useful contrast to the convolution case, where the
  branch was a *memory-access* guard (unvectorizable) rather than a
  *value-selection* (vectorizable via masks).
- **Gx/Gy visualization** (`main.cpp:183,193`) — simple abs+clamp, fully
  vectorized, as expected.

### 3.4 What this motivates for the RVV rewrite (hand-off to Student C)

The auto-vec report is effectively a to-do list for manual intrinsics:

1. **Kill the boundary branch.** Pre-pad the image with a zero border so the
   inner loop has no `if`, then process only the interior with vector loads.
   This is precisely the control flow the compiler refused to vectorize.
2. **Vectorize L2 by hand.** A manual RVV `vfsqrt` (or a fixed-point integer
   approximation) captures the gain the auto-vectorizer left on the table —
   the highest-impact single change in the whole project.
3. **Use masking for the tail**, not a scalar boundary `if`, so the kernel stays
   vector-length-agnostic across VLEN 128/256/512.
4. **Annotate `__restrict__`** where applicable to avoid the aliasing-versioning
   overhead seen in Mag L1.

> Predicted, confirmable: once the boundary check is removed and Gaussian is
> vectorized with RVV, the VLEN sweep should finally show wider VLEN getting
> *faster*. In the current scalar build it is flat (Gaussian ≈ 6.4 ms at every
> VLEN in Student A's QEMU experiments) precisely because Gaussian emits no
> vector instructions. That flatness is the experimental signature of the
> auto-vectorization failure documented above.

---

## 4. One-paragraph summary (drop-in for the report)

> The scalar pipeline was swept across `-O0/-O2/-O3/-Os/-Ofast`. `-O2` was
> fastest (25.3 ms, 3.1× over `-O0`); `-O3` was 13% *slower* because the hot
> convolution stages cannot be auto-vectorized, so its extra inlining only
> inflates QEMU's instruction count. `-fopt-info-vec-all` confirms the cause:
> the boundary-guard branch inside the Gaussian and Sobel inner loops is
> reported as *"unsupported control flow in loop,"* leaving both stages fully
> scalar, while the `double sqrt` in L2 magnitude is rejected as an
> *"unsupported data-type."* The compiler did auto-vectorize the two magnitude-L1
> passes and the direction loop (129 `vset` instructions total). These findings
> set the RVV priority order — L2 magnitude and Gaussian first — and justify the
> manual-intrinsic strategy of zero-padding to remove the branch and using
> masked tail handling for vector-length agnosticism.
