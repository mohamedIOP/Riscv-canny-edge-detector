# RISC-V Vector (RVV 1.0) Canny Edge Detector

This project implements a Canny Edge Detection pipeline targeting RISC-V (rv64gcv) and running on QEMU in user-mode emulation. The optimization journey goes from a clean scalar C++ baseline through compiler optimizations to hand-written RVV intrinsics.

This guide allows any newcomer to clone, build, test, and run the project from scratch.

---

## 🛠️ Prerequisites: Install WSL (Windows Users)

If you are on Windows and do not have a Linux environment, use Windows Subsystem for Linux (WSL).

1. Open **PowerShell** as Administrator and run:
```powershell
wsl --install
```
2. Restart your computer if prompted.
3. Open the new "Ubuntu" app and set up your UNIX username and password.
4. Update your system:
```bash
sudo apt update && sudo apt upgrade -y
```

---

## 🚀 Environment Setup

### Step 1: Install System Dependencies
```bash
sudo apt update
sudo apt install -y autoconf automake build-essential bison flex texinfo gperf \
    libtool patchutils bc cmake libglib2.0-dev libpixman-1-dev libslirp-dev \
    ninja-build libmpc-dev libmpfr-dev libgmp-dev zlib1g-dev libexpat-dev
```

### Step 2: Build the RISC-V Toolchain
> ⚠️ This compiles GCC from scratch — takes 30–60 minutes. Grab a coffee!

```bash
cd ~
git clone https://github.com/riscv-collab/riscv-gnu-toolchain \
    --recursive --depth 1 --shallow-submodules
cd riscv-gnu-toolchain
./configure --prefix=$HOME/riscv-toolchain --with-arch=rv64gcv --with-abi=lp64d
make -j$(nproc)
```

**Troubleshooting:**
- If you see `Killed` or intermediate file deletions → run `make -j2` instead
- If WSL shuts down suddenly → reopen and re-run `./configure` then `make -j2`

### Step 3: Build QEMU
```bash
cd ~
git clone https://github.com/qemu/qemu --depth 1
cd qemu
./configure --target-list=riscv64-linux-user --enable-plugins --prefix=$HOME/qemu-install
make -j$(nproc)
make install
```

**Troubleshooting:**
- If you see "python venv creation failed":
```bash
sudo apt install -y python3-venv
./configure --target-list=riscv64-linux-user --enable-plugins
make -j$(nproc)
sudo make install
```

### Step 4: Add Tools to PATH
```bash
echo 'export PATH="$HOME/riscv-toolchain/bin:$HOME/qemu-install/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

**Verify:**
```bash
qemu-riscv64 --version        # should report QEMU 9.x or newer
riscv64-unknown-elf-g++ --version  # should report GCC 13.x, 14.x, or newer
```

---

## 💻 Working with the Project

### 1. Clone the Repository
```bash
git clone https://github.com/mohamedIOP/Riscv-canny-edge-detector
cd Riscv-canny-edge-detector
```

### 2. Generate a Test Image
Generate a 256×256 synthetic gradient image:
```bash
python3 generate_image.py
```
This creates `Input_Images/input.raw` (65536 bytes = 256×256).

To use your own photo instead:
```bash
pip3 install Pillow --break-system-packages
python3 convert_image.py your_photo.jpg Input_Images/input.raw 256 256
```

### 3. Build the RISC-V Binary
```bash
make clean
make
```

### 4. Run the Full Pipeline on QEMU
```bash
make run
```
This runs the complete Canny pipeline at **VLEN=128, 256, and 512**, prints a **per-stage timing report** (100 iterations for stability), and saves all stage outputs to `Output_Images/`.

| Output file | Contents |
|---|---|
| `output_gaussian.raw` | After Gaussian blur (5×5) |
| `output_sobel_gx.raw` | Sobel gradient — X direction |
| `output_sobel_gy.raw` | Sobel gradient — Y direction |
| `output_magnitude_l1.raw` | Edge strength — L1 norm |
| `output_128.raw` | Edge strength — L2 norm (VLEN=128 run) |
| `output_256.raw` | Edge strength — L2 norm (VLEN=256 run) |
| `output_512.raw` | Edge strength — L2 norm (VLEN=512 run) |
| `output_direction.raw` | Gradient direction (0/85/170/255) |
| `output_nms.raw` | **Bonus** — non-maximum suppression (thinned edges) |
| `output_threshold.raw` | **Bonus** — double threshold (0 / 128 / 255) |
| `output_edges.raw` | **Bonus** — final Canny edges (binary 0/255) |

---

## 🔬 Viewing Results

Install dependencies:
```bash
pip3 install numpy matplotlib --break-system-packages
```

**View all pipeline stages at once:**
```bash
python3 view_image.py stages 256 256
```

**Compare VLEN=128 / 256 / 512 outputs (proves vector-length-agnostic correctness):**
```bash
python3 view_image.py 256 256
```
Should print: `✅ All three VLEN outputs are IDENTICAL — pipeline is vector-length-agnostic!`

**View a single file:**
```bash
python3 view_image.py Output_Images/output_gaussian.raw 256 256
```

---

## ✅ Correctness Verification (OpenCV Reference)

`reference_opencv.py` is an independent reference implementation of every scalar stage using OpenCV / NumPy. It re-derives Gaussian, Sobel, L1/L2 magnitude, and direction from the same input and compares them against the pipeline's `.raw` outputs, printing a per-stage PASS/FAIL table (max / mean / exact% / within-tol%).

```bash
pip3 install opencv-python-headless --break-system-packages
# after `make run` has populated Output_Images/:
python3 reference_opencv.py Input_Images/input.raw 256 256
```

> ⚠️ **Note:** Do NOT include `numpy` in the pip install command. OpenCV will use your existing NumPy. Installing `numpy` explicitly may upgrade to NumPy 2.x which breaks system Matplotlib. If you encounter NumPy version errors, downgrade with: `pip3 install 'numpy<2' --break-system-packages --force-reinstall`

It uses two references on purpose: a **faithful** one (the project's exact integer `/273` Gaussian kernel + `cv2.Sobel`, zero-padded) which must match the pipeline to within ±1 LSB and is the PASS/FAIL gate, and an **informational** `cv2.GaussianBlur(sigma=1.0)` library reference (different float kernel, reported but never fails the build). Reference images and amplified difference heatmaps are written to `Reference_Images/`. The script returns a non-zero exit code on mismatch, so it can be dropped straight into CI. Options: `--tol`, `--outdir`, `--refdir`, `--l2`, `--no-save`, `--no-diff`, `--quiet`.

---

## 🏆 Bonus: Full Canny Pipeline (NMS + Thresholding + Hysteresis)

Beyond the minimum deliverable (Gaussian + Sobel), the pipeline implements the
remaining three Canny stages, all in clean scalar C++:

- **Stage 3 — Non-Maximum Suppression** (`Phase 2/src/nms.cpp`): thins the
  gradient magnitude to single-pixel ridges by keeping a pixel only if it is a
  local maximum against the two neighbours lying along its quantized gradient
  direction (0/1/2/3 from the direction stage).
- **Stage 4 — Double Threshold** (`Phase 2/src/threshold.cpp`): classifies each
  pixel as strong (≥ high), weak (≥ low), or none, using thresholds
  `low=20, high=50` on the normalized magnitude.
- **Stage 5 — Hysteresis** (`Phase 2/src/threshold.cpp`): an explicit
  stack-based (non-recursive) 8-connected flood fill that promotes weak pixels
  connected to a strong edge and discards the rest, producing a binary edge map.

These stages are **scalar by design**: their control flow is data-dependent
(neighbour choice branches on direction; hysteresis is a flood fill), so they
are not auto-vectorization or RVV-intrinsic targets — consistent with the
profile-then-optimize philosophy used throughout the project. They are covered
by host GoogleTest unit tests, QEMU-side property/determinism tests, and the
OpenCV reference verifier (all three reproduced faithfully in
`reference_opencv.py`).

The CI workflow (`.github/workflows/ci.yml`) builds the project and runs the
host GoogleTest suite on every push (the **+1 CI bonus**).

Run the pipeline natively on your PC (no QEMU needed) for fast visual debugging:
```bash
make visual
./visual_pipeline 256 256 Input_Images/input.raw
```
Outputs saved to `Output_Images/`.

---

## ✅ Run Unit Tests

### Host-Side Tests (GoogleTest)
```bash
make test
```
GoogleTest runs natively on the host. All pipeline stages are tested for correctness (uniform image invariant, impulse response, edge direction, magnitude).

### QEMU-Side Tests (Equivalence & Invariants)
```bash
make test_qemu
```
Runs 45 assertions on QEMU at VLEN=128, 256, and 512:
- Scalar invariants (uniform image, impulse response, edge direction)
- Tail-case stress tests (17×13, 33×29, 100×75, 101×77)
- **RVV vs Scalar equivalence** (Gaussian and Magnitude L1 compared with ±1 tolerance)
- Vector-length-agnostic verification

---

## ⏱️ Profiling and Optimization

### Compiler Flag Sweep
Run the compiler flag sweep to measure performance at each optimization level:
```bash
make sweep      # builds binaries at -O0, -O2, -O3, -Os, -Ofast
make run_sweep  # runs each binary and prints per-stage timing
```
Results are saved in `docs/optimization_results.md`.

### Auto-Vectorization Analysis
To reproduce the auto-vectorization analysis:
```bash
riscv64-unknown-elf-g++ -static -march=rv64gcv -mabi=lp64d -O3 -std=c++17 \
    -I"Phase 2/include" -fopt-info-vec-all \
    main.cpp "Phase 2"/src/*.cpp -o canny_vec_report 2>&1 | tee vec_report.txt

riscv64-unknown-elf-objdump -d canny_vec_report | grep -c "vset"
```

### Separable Filter Experiment
Compare 2D Gaussian (25 muls/pixel) vs separable (10 muls/pixel):
```bash
make separable
```
Results saved to `separable_results.txt` (5.83× speedup observed).

### Pre-Padding Experiment
Test if removing boundary checks enables compiler auto-vectorization:
```bash
make prepad_experiment
```
Results saved to `prepad_results.txt` (9.02× speedup observed).

### Auto-Vectorization Investigation
Run both prepad and separable experiments together:
```bash
make autovec_investigation
```

---

## 📊 RVV Intrinsic Optimization

### LMUL Experiments
Test different vector length multipliers (LMUL=1, 2, 4) for RVV kernels:
```bash
make lmul_experiment
```
Results documented in `LMUL_RESULTS.md` (LMUL=4 identified as sweet spot).

### RVV Code Walkthrough
The RVV implementations are in:
- `Phase 2/src/gaussian.cpp` — `gaussian_blur_5x5_rvv()` with strip-mining, fixed-point division
- `Phase 2/src/magnitude.cpp` — `magnitude_l1_rvv()` with vector reduction (`vredmax`)

Every `__riscv_*` intrinsic is annotated with:
1. **What operation it performs**
2. **Why this LMUL was chosen**
3. **How it behaves across different VLEN values**

---

## 📁 Project Structure

```
.
├── .github/
│   └── workflows/
│       └── ci.yml                          # CI: builds toolchain, QEMU, runs tests, VLEN sweep
├── docs/
│   └── optimization_results.md             # Full optimization report with all data
├── Important_Results/                      # Result screenshots and images
├── Input_Images/                           # Input raw images (not committed)
├── Output_Images/                          # Pipeline outputs (not committed)
├── Phase 2/
│   ├── include/
│   │   ├── convolution.hpp                 # Generic template-based convolution kernel
│   │   ├── direction.hpp                   # Gradient direction interface
│   │   ├── gaussian.hpp                    # Gaussian blur (scalar + separable + RVV)
│   │   ├── magnitude.hpp                   # Magnitude L1/L2 (scalar + RVV)
│   │   └── sobel.hpp                       # Sobel gradient (Gx/Gy) interface
│   └── src/
│       ├── direction.cpp                   # Scalar direction implementation
│       ├── gaussian.cpp                    # Scalar 2D, separable, and RVV Gaussian
│       ├── magnitude.cpp                   # Scalar L1/L2 and RVV L1 magnitude
│       └── sobel.cpp                       # Scalar Sobel Gx/Gy implementation
├── src/
│   ├── pipeline.cpp                        # Generic convolve2D template (reference)
│   ├── pipeline.hpp                        # Template interface
│   └── profiler.hpp                        # Bare-metal clock_gettime profiling harness
├── tests/
│   ├── qemu_equivalence_test.cpp           # QEMU-side: scalar invariants + RVV equivalence (45 tests)
│   └── test_pipeline.cpp                   # Host-side GoogleTest (14+ unit tests)
├── main.cpp                                # RISC-V entry point: pipeline + profiling + RVV speedup
├── visual_pipeline.cpp                     # Native host pipeline for visualization
├── Makefile                                # Dual-target build system
├── generate_image.py                       # Generate synthetic test images
├── convert_image.py                        # Convert any photo to raw grayscale
├── view_image.py                           # Visualize raw output files
├── reference_opencv.py                     # OpenCV reference verifier (PASS/FAIL per stage)
├── LMUL_RESULTS.md                         # LMUL=1/2/4 performance sweep data
├── prepad_results.txt                      # Pre-padding experiment results
├── separable_results.txt                   # Separable filter benchmark results
├── vec_report.txt                          # Raw auto-vectorization compiler output
└── readme.md                               # This file
```

---

## 📊 Optimization Results

Profiling data, compiler flag sweep timings, binary sizes, auto-vectorization analysis, QEMU configuration experiments, and RVV intrinsic results are documented in `docs/optimization_results.md`.

Key findings:
- **`-O2` is the sweet spot** (25.32 ms) — faster than `-O3` (28.71 ms) due to instruction cache pressure
- **Gaussian and Sobel are not auto-vectorized** — blocked by boundary check control flow
- **Magnitude L1 and Direction are partially auto-vectorized** — 129 vset instructions found
- **Separable filter: 5.83× speedup** over 2D convolution
- **Pre-padding: 9.02× speedup** — proves boundary checks prevent vectorization
- **RVV Gaussian: 0.12–0.39× on QEMU** (slower due to emulation overhead, would be 2–4× on real hardware)
- **RVV Magnitude L1: 1.59× speedup** at VLEN=128 (Magnitude L1)
- **LMUL=4 is the sweet spot** for RVV kernels
- **`-Os` anomaly**: Magnitude L1 scalar shows 0.54 ms (6× faster than `-O2`'s 3.41 ms) — output verified bit-identical, likely due to QEMU's emulated instruction cache favoring smaller code

---

## 🎯 Makefile Targets

| Command | Description |
|---|---|
| `make` | Build RISC-V binary at -O2 (fastest per our profiling sweep) |
| `make run` | Run full pipeline on QEMU at VLEN 128, 256, 512 with per-stage timing |
| `make test` | Run GoogleTest suite natively |
| `make test_qemu` | Run QEMU equivalence tests at VLEN 128, 256, 512 (45 assertions) |
| `make visual` | Build native host pipeline binary |
| `make sweep` | Build binaries at -O0, -O2, -O3, -Os, -Ofast and print sizes |
| `make run_sweep` | Run timing measurements at all optimization levels |
| `make qemu_eq_test` | Build QEMU equivalence test binary |
| `make separable` | Run separable filter benchmark |
| `make prepad_experiment` | Run pre-padding auto-vectorization experiment |
| `make separable_autovec` | Run separable filter auto-vectorization analysis |
| `make autovec_investigation` | Run both prepad and separable experiments |
| `make lmul_experiment` | Run LMUL=1/2/4 performance sweep |
| `make clean` | Remove all binaries and output .raw files |

---

## 👥 Team & AI Usage

This project was developed by a team of three students with the following responsibilities:

| Student | Role | Focus |
|---|---|---|
| **Student A** | Infrastructure | Toolchain, QEMU, Makefile, CI, profiling harness, compiler sweep |
| **Student B** | Pipeline | Scalar pipeline, OpenCV reference, separable filter, auto-vectorization analysis, report writing |
| **Student C** | Testing & Vectorization | GoogleTest, RVV intrinsics, LMUL experiments, equivalence tests |

### AI Usage Log

We used AI assistants (ChatGPT, Claude, GitHub Copilot) throughout the project. Below is a documented log of our interactions:

| # | Question Asked | AI Suggestion | What We Changed | Reflection |
|---|---|---|---|---|
| 1 | "RISC-V toolchain build fails with 'Killed' during compilation" | Reduce parallel jobs from `-j$(nproc)` to `-j2` to avoid WSL memory limits | Changed Makefile and README to recommend `make -j2` for WSL users | Memory exhaustion was the root cause; this fix is now in our troubleshooting guide |
| 2 | "What is the correct RVV intrinsic for loading uint8 and widening to uint16?" | Use `vle8_v_u8mf2` followed by `vwcvtu_x_x_v_u16m1` for the widening chain | Implemented the exact widening chain in `gaussian_blur_5x5_rvv()`: u8mf2 → u16m1 → u32m2 | Verified against RVV spec v1.0; LMUL doubles at each widening step |
| 3 | "How to replace slow vector division by 273 with fixed-point arithmetic?" | Use `(sum * 240) >> 16` as approximation for `sum / 273` (error < 0.02%) | Replaced `vdiv` with `vmul` + `vsrl` in the RVV Gaussian kernel | Eliminates the slowest vector instruction; output verified identical to scalar division |
| 4 | "Makefile design pattern for dual-target build (host g++ vs RISC-V cross-compiler)" | Use separate `CXX` and `CXX_RISCV` variables with conditional compilation | Implemented with `make` (RISC-V) and `make test` (host) targets | Clean separation; no source code duplication |
| 5 | "How to structure the optimization results report with Amdahl's Law prioritization?" | Create per-stage percentage breakdown table, rank by % of total time | Added profiling harness with `clock_gettime` and percentage columns in timing table | Profiling data confirmed Gaussian as #1 hotspot, justifying RVV effort there first |

All AI suggestions were verified against the RISC-V Vector Extension specification and tested on QEMU before integration.

---

## 🙏 Acknowledgments

- RISC-V GNU Toolchain: https://github.com/riscv-collab/riscv-gnu-toolchain
- QEMU: https://github.com/qemu/qemu
- GoogleTest: https://github.com/google/googletest
