# RISC-V Vector (RVV 1.0) Canny Edge Detector

This project implements a Canny Edge Detection algorithm utilizing the RISC-V "V" (Vector) Extension (RVV 1.0). Because standard package managers do not reliably support the RVV 1.0 intrinsics, **you must build the toolchain and emulator from source** to work on this project.

This guide allows any newcomer to clone, build, test, and run the project completely from scratch.

---

## 🛠️ Prerequisites: Install WSL (Windows Users)
If you are on Windows and do not have a Linux environment, you must use Windows Subsystem for Linux (WSL).

1. Open **PowerShell** as Administrator.
2. Run the following command:
   ```powershell
   wsl --install
   ```
3. Restart your computer if prompted.
4. Open the new "Ubuntu" app from your Start menu and set up your UNIX username and password.
5. Update your system by running:
   ```bash
   sudo apt update && sudo apt upgrade -y
   ```

---

## 🚀 Environment Setup Guide

### Step 1: Install System Dependencies
Install the required packages to build the compiler and emulator from source.
```bash
sudo apt update
sudo apt install -y autoconf automake build-essential bison flex texinfo gperf libtool patchutils bc cmake libglib2.0-dev libpixman-1-dev libslirp-dev ninja-build libmpc-dev libmpfr-dev libgmp-dev zlib1g-dev libexpat-dev
```

### Step 2: Build the Custom RISC-V Toolchain
*(⚠️ **WARNING:** This step compiles GCC from scratch. Depending on your CPU, it will take 30 to 60 minutes. Grab a coffee!)*

Clone the toolchain into your home directory and compile it for bare-metal with Vector extensions (`rv64gcv`):
```bash
cd ~
git clone https://github.com/riscv-collab/riscv-gnu-toolchain --recursive --depth 1 --shallow-submodules
cd riscv-gnu-toolchain
./configure --prefix=$HOME/riscv-toolchain --with-arch=rv64gcv --with-abi=lp64d
make -j$(nproc)
```
If there is a problem after configure line with "python venv creation failed" just run
```bash
sudo apt install -y python3-venv
```
Then run 
```bash
./configure --target-list=riscv64-linux-user --enable-plugins
make -j$(nproc)
sudo make install
```
### Step 3: Build QEMU Emulator
We need QEMU to emulate the RISC-V vector instructions locally.
```bash
cd ~
git clone https://github.com/qemu/qemu --depth 1
cd qemu
./configure --target-list=riscv64-linux-user --enable-plugins --prefix=$HOME/qemu-install
make -j$(nproc)
make install
```

### Step 4: Add Tools to Your PATH
To let your terminal know where the newly built tools are, add them to your environment variables. Run this command to append it to your `.bashrc` profile:
```bash
echo 'export PATH="$HOME/riscv-toolchain/bin:$HOME/qemu-install/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

---

## 💻 Working with the Project

### 1. Clone the Repository
```bash
git clone https://github.com/mohamedIOP/Riscv-canny-edge-detector
cd Riscv-canny-edge-detector
```

### 2. Generate a Test Image
**Do not commit raw images to the repository.** Before running tests, generate a dummy test image using the terminal:
```bash
dd if=/dev/urandom of=input.raw bs=1 count=65536
```

### 3. Compile and Run
Compile the C++ code using the custom bare-metal compiler:
```bash
make clean
make
```

Run the code through the QEMU emulator:
```bash
make run
```
If successful, the terminal will print out the simulated Vector Length (VLEN) and execute the program!

---
