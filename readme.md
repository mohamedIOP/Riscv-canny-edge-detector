# RISC-V Vector (RVV 1.0) Canny Edge Detector

This project implements a Canny Edge Detection pipeline targeting RISC-V (rv64gcv)
and running on QEMU in user-mode emulation. The optimization journey goes from a
clean scalar C++ baseline through compiler optimizations to hand-written RVV intrinsics.

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
This runs the complete Canny pipeline at **VLEN=128, 256, and 512**, prints a
**per-stage timing report** (100 iterations for stability), and saves all stage
outputs to `Output_Images/`.

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

## 🖼️ Visual Pipeline (Native Host)

Run the pipeline natively on your PC (no QEMU needed) for fast visual debugging:
```bash
make visual
./visual_pipeline 256 256 Input_Images/input.raw
```
Outputs saved to `Output_Images/`.

---

## ✅ Run Unit Tests
```bash
make test
```
GoogleTest runs natively on the host. All pipeline stages are tested for
correctness (uniform image invariant, impulse response, edge direction, magnitude).

---
## ⏱️ Profiling and Optimization

Run the compiler flag sweep to measure performance at each optimization level:
```bash
make sweep      # builds binaries at -O0, -O2, -O3, -Os, -Ofast
make run_sweep  # runs each binary and prints per-stage timing
```

Results are saved in `docs/optimization_results.md`.

To reproduce the auto-vectorization analysis:
```bash
riscv64-unknown-elf-g++ -static -march=rv64gcv -mabi=lp64d -O3 -std=c++17 \
    -I"Phase 2/include" -fopt-info-vec-all \
    main.cpp "Phase 2"/src/*.cpp -o canny_vec_report 2>&1 | tee vec_report.txt

riscv64-unknown-elf-objdump -d canny_vec_report | grep -c "vset"
```
---
## 📁 Project Structure

```
.
├── main.cpp                      # RISC-V entry point — full pipeline + profiling
├── visual_pipeline.cpp           # Native host pipeline for visualization
├── Makefile                      # Dual-target: RISC-V + host, compiler sweep
├── generate_image.py             # Generates 256×256 synthetic test image
├── convert_image.py              # Converts any photo to raw grayscale
├── view_image.py                 # Visualizes raw output files
├── src/
│   ├── pipeline.cpp              # Generic 2D convolution template
│   ├── pipeline.hpp              # Convolution template interface
│   └── profiler.hpp              # Per-stage timing harness (clock_gettime)
├── Phase 2/
│   ├── include/                  # Headers: gaussian, sobel, magnitude, direction, convolution
│   └── src/                     # Scalar C++ implementations
├── tests/
│   ├── test_pipeline.cpp         # GoogleTest unit tests (host-side)
│   └── qemu_equivalence_test.cpp # QEMU-side equivalence tests at VLEN 128/256/512
├── docs/
│   └── optimization_results.md  # Profiling data, flag sweep, auto-vec, RVV results
├── vec_report.txt                # Raw auto-vectorization compiler output
├── Input_Images/                 # Input raw images (not committed)
└── Output_Images/                # Pipeline outputs (not committed)
```
---
## 📊 Optimization Results

Profiling data, compiler flag sweep timings, and binary sizes are documented in
`docs/optimization_results.md`. This file will be updated as RVV intrinsic
implementations are completed.

## 🎯 Makefile Targets

| Command | Description |
|---|---|
| `make` | Build RISC-V binary at -O2 (fastest per our profiling sweep) |
| `make run` | Run full pipeline on QEMU at VLEN 128, 256, 512 with per-stage timing |
| `make test` | Run GoogleTest suite natively |
| `make test_qemu` | Run QEMU equivalence tests at VLEN 128, 256, 512 |
| `make visual` | Build native host pipeline binary |
| `make sweep` | Build binaries at -O0, -O2, -O3, -Os, -Ofast and print sizes |
| `make run_sweep` | Run timing measurements at all optimization levels |
| `make qemu_eq_test` | Build QEMU equivalence test binary |
| `make clean` | Remove all binaries and output .raw files |