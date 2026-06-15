#!/usr/bin/env python3
"""
reference_opencv.py  --  OpenCV reference & correctness verifier
================================================================
Student B deliverable: standalone Python/OpenCV reference for the Canny
pipeline. Re-implements every scalar stage with OpenCV / NumPy and compares
the result against the RISC-V pipeline's `.raw` outputs.

WHY THIS EXISTS
---------------
view_image.py only *displays* raw files. It cannot tell you whether the C++
output is numerically correct. This script provides an independent reference:
if the RISC-V pipeline and OpenCV agree (within rounding tolerance) on the same
input, the scalar baseline is trustworthy and can be used as the golden
reference for the RVV equivalence tests.

TWO REFERENCES, ON PURPOSE
--------------------------
  1. FAITHFUL reference  -> reproduces the project's exact integer math:
       * Gaussian: the exact {1,4,7,...}/273 integer kernel, floor division.
       * Sobel:    cv2.Sobel (CV_16S, ksize=3) IS the project's exact kernel.
       * Zero-padding boundaries (cv2.BORDER_CONSTANT, value 0).
     This should match the pipeline to within +/-1 LSB (pure rounding). It is
     the PASS/FAIL correctness gate.

  2. LIBRARY reference   -> idiomatic cv2.GaussianBlur(sigma=1.0).
     OpenCV builds its own float Gaussian kernel, so this will NOT match the
     project's integer kernel bit-for-bit. It is reported for information only
     (algorithmic sanity check), never used to fail the build.

MATCHING NOTES (verified against the C++ source)
------------------------------------------------
  * Raw format: width*height bytes, row-major, reshape(H, W). uint8.
  * Boundaries: zero-padding everywhere  -> cv2.BORDER_CONSTANT.
  * Gaussian:   integer accumulate, `sum /= 273` (C++ truncation == floor for
                non-negative sums), then clamp [0,255].
  * Sobel:      computed on the BLURRED image; saved as clamp(|G|,255) uint8.
  * Magnitude:  two-pass, normalized by the global max -> (m*255)/max.
  * Direction:  integer cross-multiply quantization (0/1/2/3), saved as dir*85.

USAGE
-----
  python3 reference_opencv.py <input.raw> <width> <height> [options]

  options:
    --outdir DIR     pipeline output dir to compare against (default Output_Images)
    --refdir DIR     where to write reference outputs   (default Reference_Images)
    --l2 FILE        pipeline's L2 magnitude file (default: auto-detect
                     output_128/256/512.raw or output_magnitude_l2.raw in outdir)
    --tol N          per-pixel tolerance in LSB for PASS (default 1)
    --no-save        do not write reference .raw / .png files
    --no-diff        do not write difference heatmaps
    --quiet          only print the summary table

  exit code: 0 if every faithful stage passes within --tol, else 1 (CI-friendly).

Author: Student B  |  Dependencies: opencv-python(-headless), numpy, (matplotlib optional)
"""

import sys
import os
import argparse
import numpy as np
import cv2

# --------------------------------------------------------------------------
# Project constants (copied verbatim from the C++ source so the reference
# stays in lock-step with the pipeline; edit here if the kernels ever change).
# --------------------------------------------------------------------------
GAUSSIAN_5X5 = np.array([
    [1,  4,  7,  4, 1],
    [4, 16, 26, 16, 4],
    [7, 26, 41, 26, 7],
    [4, 16, 26, 16, 4],
    [1,  4,  7,  4, 1],
], dtype=np.int32)
GAUSSIAN_DIVISOR = 273  # == GAUSSIAN_5X5.sum()

SOBEL_X = np.array([[-1, 0, 1],
                    [-2, 0, 2],
                    [-1, 0, 1]], dtype=np.int32)
SOBEL_Y = np.array([[-1, -2, -1],
                    [ 0,  0,  0],
                    [ 1,  2,  1]], dtype=np.int32)

# tan(22.5 deg) ~ 2/5, tan(67.5 deg) ~ 12/5  (same integer ratios as direction.cpp)
DIR_VIS_SCALE = 85  # dir in {0,1,2,3} -> {0,85,170,255}


# ==========================================================================
# Reference stage implementations
# ==========================================================================
def ref_gaussian_faithful(img):
    """Exact integer 5x5 Gaussian with floor division and zero-padding.
    Mirrors canny::convolve<uint8,int32,int16>(..., 273)."""
    # filter2D needs a float kernel; the convolution sums here (<= 255*273 =
    # 69615) are small integers that float64 represents *exactly*, so casting
    # the result back to int64 recovers the exact integer accumulator. We then
    # apply integer floor-division by 273 -- identical to the C++ `sum /= 273`.
    # BORDER_CONSTANT == zero-padding (matches the project boundary handling).
    acc = cv2.filter2D(img.astype(np.float64), cv2.CV_64F,
                       GAUSSIAN_5X5.astype(np.float64),
                       borderType=cv2.BORDER_CONSTANT)
    acc = np.rint(acc).astype(np.int64)    # exact integer accumulator
    out = acc // GAUSSIAN_DIVISOR          # C++ integer truncation (sum >= 0 -> floor)
    return np.clip(out, 0, 255).astype(np.uint8)


def ref_gaussian_library(img):
    """Idiomatic cv2.GaussianBlur reference (float kernel, sigma=1.0).
    NOT bit-identical to the project kernel; informational only."""
    return cv2.GaussianBlur(img, (5, 5), sigmaX=1.0, sigmaY=1.0,
                            borderType=cv2.BORDER_CONSTANT)


def ref_sobel(blurred):
    """Sobel Gx, Gy on the blurred image. cv2.Sobel with ksize=3 uses exactly
    the project's kernels, so this is bit-faithful (not just 'library')."""
    gx = cv2.Sobel(blurred, cv2.CV_16S, 1, 0, ksize=3,
                   borderType=cv2.BORDER_CONSTANT)
    gy = cv2.Sobel(blurred, cv2.CV_16S, 0, 1, ksize=3,
                   borderType=cv2.BORDER_CONSTANT)
    return gx.astype(np.int32), gy.astype(np.int32)


def sobel_vis(g):
    """Reproduce main.cpp visualization: clamp(|G|, 255) as uint8."""
    return np.clip(np.abs(g), 0, 255).astype(np.uint8)


def ref_magnitude_l1(gx, gy):
    """|Gx| + |Gy|, normalized to [0,255] by the global max (two-pass)."""
    m = np.abs(gx) + np.abs(gy)
    mx = int(m.max())
    if mx == 0:
        return np.zeros_like(gx, dtype=np.uint8)
    # integer math matching the C++: (m * 255) / max, truncated
    return ((m * 255) // mx).astype(np.uint8)


def ref_magnitude_l2(gx, gy):
    """sqrt(Gx^2 + Gy^2), normalized to [0,255] by the global max (two-pass)."""
    m = np.sqrt(gx.astype(np.float64) ** 2 + gy.astype(np.float64) ** 2)
    mx = m.max()
    if mx == 0.0:
        return np.zeros_like(gx, dtype=np.uint8)
    return ((m * 255.0) / mx).astype(np.uint8)  # truncation matches (uint8) cast


def ref_direction(gx, gy):
    """Quantized direction 0/1/2/3 via integer cross-multiplication, exactly as
    direction.cpp. Returned as the *_vis form (dir*85) for raw comparison."""
    ax = np.abs(gx)
    ay = np.abs(gy)

    d = np.full(gx.shape, 1, dtype=np.uint8)            # default diagonal (45)
    horizontal = (5 * ay) < (2 * ax)                    # -> 0 deg
    vertical   = (5 * ay) >= (12 * ax)                  # -> 90 deg
    # anti-diagonal: in the diagonal band and x,y have opposite signs
    diag_band  = ~horizontal & ~vertical
    same_sign  = ((gx >= 0) & (gy >= 0)) | ((gx < 0) & (gy < 0))
    d[diag_band & same_sign]  = 1
    d[diag_band & ~same_sign] = 3
    d[horizontal] = 0
    d[vertical]   = 2
    return (d * DIR_VIS_SCALE).astype(np.uint8)


# ==========================================================================
# Comparison helpers
# ==========================================================================
def load_raw(path, w, h):
    if not os.path.exists(path):
        return None
    if os.path.getsize(path) != w * h:
        print(f"  ! size mismatch for {path} "
              f"({os.path.getsize(path)} vs {w*h}) -- skipped")
        return None
    return np.fromfile(path, dtype=np.uint8).reshape(h, w)


def compare(name, ref, got, tol):
    """Return a dict of comparison metrics, or None if `got` is missing."""
    if got is None:
        return None
    diff = np.abs(ref.astype(np.int32) - got.astype(np.int32))
    total = diff.size
    return {
        "name":   name,
        "max":    int(diff.max()),
        "mean":   float(diff.mean()),
        "exact":  100.0 * np.count_nonzero(diff == 0) / total,
        "within": 100.0 * np.count_nonzero(diff <= tol) / total,
        "pass":   int(diff.max()) <= tol,
        "diff":   diff.astype(np.uint8),
    }


def save_png(path, arr):
    cv2.imwrite(path, arr)


def save_diff_png(path, diff):
    """Amplified heatmap so small differences are visible."""
    amp = np.clip(diff.astype(np.int32) * 32, 0, 255).astype(np.uint8)
    cm = cv2.applyColorMap(amp, cv2.COLORMAP_JET)
    cm[diff == 0] = (0, 0, 0)  # exact-match pixels stay black
    cv2.imwrite(path, cm)


# ==========================================================================
# Main
# ==========================================================================
def main():
    ap = argparse.ArgumentParser(
        description="OpenCV reference & correctness verifier for the RISC-V Canny pipeline.")
    ap.add_argument("input"); ap.add_argument("width", type=int)
    ap.add_argument("height", type=int)
    ap.add_argument("--outdir", default="Output_Images")
    ap.add_argument("--refdir", default="Reference_Images")
    ap.add_argument("--l2", default=None)
    ap.add_argument("--tol", type=int, default=1)
    ap.add_argument("--no-save", action="store_true")
    ap.add_argument("--no-diff", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()

    W, H = args.width, args.height
    if not os.path.exists(args.input):
        print(f"ERROR: input not found: {args.input}")
        return 2
    if os.path.getsize(args.input) != W * H:
        print(f"ERROR: input is {os.path.getsize(args.input)} bytes, "
              f"expected {W*H} for {W}x{H}.")
        return 2

    img = np.fromfile(args.input, dtype=np.uint8).reshape(H, W)

    def log(*a):
        if not args.quiet:
            print(*a)

    log("=" * 64)
    log(f"OpenCV reference  |  input={args.input}  {W}x{H}  tol=+/-{args.tol} LSB")
    log("=" * 64)

    # ---- compute the faithful reference end-to-end -----------------------
    g_faithful = ref_gaussian_faithful(img)
    g_library  = ref_gaussian_library(img)
    gx, gy     = ref_sobel(g_faithful)        # Sobel runs on the faithful blur
    gx_vis     = sobel_vis(gx)
    gy_vis     = sobel_vis(gy)
    mag_l1     = ref_magnitude_l1(gx, gy)
    mag_l2     = ref_magnitude_l2(gx, gy)
    dir_vis    = ref_direction(gx, gy)

    references = {
        "ref_gaussian.raw":       g_faithful,
        "ref_gaussian_cv2.raw":   g_library,
        "ref_sobel_gx.raw":       gx_vis,
        "ref_sobel_gy.raw":       gy_vis,
        "ref_magnitude_l1.raw":   mag_l1,
        "ref_magnitude_l2.raw":   mag_l2,
        "ref_direction.raw":      dir_vis,
    }

    # ---- save reference outputs -----------------------------------------
    if not args.no_save:
        os.makedirs(args.refdir, exist_ok=True)
        for fname, arr in references.items():
            arr.tofile(os.path.join(args.refdir, fname))
            save_png(os.path.join(args.refdir, fname.replace(".raw", ".png")), arr)
        log(f"-> wrote {len(references)} reference files to {args.refdir}/")

    # ---- locate the pipeline's L2 output --------------------------------
    l2_path = args.l2
    if l2_path is None:
        for cand in ("output_magnitude_l2.raw", "output_128.raw",
                     "output_256.raw", "output_512.raw"):
            p = os.path.join(args.outdir, cand)
            if os.path.exists(p):
                l2_path = p
                break

    # map: pipeline file  ->  (reference array, label)
    pairs = [
        (os.path.join(args.outdir, "output_gaussian.raw"),     g_faithful, "Gaussian 5x5"),
        (os.path.join(args.outdir, "output_sobel_gx.raw"),     gx_vis,     "Sobel |Gx|"),
        (os.path.join(args.outdir, "output_sobel_gy.raw"),     gy_vis,     "Sobel |Gy|"),
        (os.path.join(args.outdir, "output_magnitude_l1.raw"), mag_l1,     "Magnitude L1"),
        (l2_path,                                              mag_l2,     "Magnitude L2"),
        (os.path.join(args.outdir, "output_direction.raw"),    dir_vis,    "Direction"),
    ]

    # ---- compare ---------------------------------------------------------
    results = []
    for path, ref, label in pairs:
        got = load_raw(path, W, H) if path else None
        res = compare(label, ref, got, args.tol)
        if res is not None:
            res["path"] = path
        results.append((label, path, res))

    if not args.no_diff and not args.no_save:
        os.makedirs(args.refdir, exist_ok=True)
        for label, path, res in results:
            if res:
                tag = label.lower().replace(" ", "_").replace("|", "")
                save_diff_png(os.path.join(args.refdir, f"diff_{tag}.png"), res["diff"])

    # ---- summary table ---------------------------------------------------
    print()
    print(f"{'Stage':<16}{'max':>6}{'mean':>9}{'exact%':>9}{'<=tol%':>9}   {'result':<8}")
    print("-" * 64)
    all_pass = True
    any_compared = False
    for label, path, res in results:
        if res is None:
            print(f"{label:<16}{'-':>6}{'-':>9}{'-':>9}{'-':>9}   (no pipeline output)")
            continue
        any_compared = True
        verdict = "PASS" if res["pass"] else "FAIL"
        if not res["pass"]:
            all_pass = False
        print(f"{label:<16}{res['max']:>6}{res['mean']:>9.3f}"
              f"{res['exact']:>8.2f}%{res['within']:>8.2f}%   {verdict:<8}")
    print("-" * 64)

    # ---- informational: cv2.GaussianBlur vs pipeline gaussian -----------
    pgauss = load_raw(os.path.join(args.outdir, "output_gaussian.raw"), W, H)
    if pgauss is not None:
        d = np.abs(g_library.astype(np.int32) - pgauss.astype(np.int32))
        print(f"[info] cv2.GaussianBlur(sigma=1.0) vs pipeline Gaussian: "
              f"max={int(d.max())}, mean={d.mean():.3f} "
              f"(expected non-zero -- different kernel, not a failure)")

    if not any_compared:
        print("\nNo pipeline outputs found to compare. Reference images were written;")
        print(f"run the pipeline (make run) so {args.outdir}/ is populated, then re-run.")
        return 0

    print(f"\nOVERALL: {'PASS -- pipeline matches OpenCV reference' if all_pass else 'FAIL -- see stages above'}")
    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
