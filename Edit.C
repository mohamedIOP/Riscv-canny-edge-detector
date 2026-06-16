# ============================================================
# Add these targets to your existing Makefile
# (after the existing targets)
# ============================================================

RV_CXX  = riscv64-unknown-elf-g++
RV_FLAGS = -static -march=rv64gcv -mabi=lp64d -O2 -std=c++17

QEMU     = qemu-riscv64
QEMU_CPU = rv64,v=true

# Build the QEMU-side equivalence test binary (cross-compiled for RISC-V)
qemu_eq_test: tests/qemu_equivalence_test.cpp \
              "Phase 2"/src/gaussian.cpp \
              "Phase 2"/src/sobel.cpp \
              "Phase 2"/src/magnitude.cpp \
              "Phase 2"/src/direction.cpp
	$(RV_CXX) $(RV_FLAGS) \
	    -I"Phase 2/include" \
	    tests/qemu_equivalence_test.cpp \
	    "Phase 2"/src/gaussian.cpp \
	    "Phase 2"/src/sobel.cpp \
	    "Phase 2"/src/magnitude.cpp \
	    "Phase 2"/src/direction.cpp \
	    -o qemu_eq_test

# Run equivalence tests at all three VLEN values
# Output must be identical at every VLEN — if not, a VLA bug exists
test_qemu: qemu_eq_test
	@echo "--- VLEN=128 ---"
	$(QEMU) -cpu $(QEMU_CPU),vlen=128 ./qemu_eq_test
	@echo "--- VLEN=256 ---"
	$(QEMU) -cpu $(QEMU_CPU),vlen=256 ./qemu_eq_test
	@echo "--- VLEN=512 ---"
	$(QEMU) -cpu $(QEMU_CPU),vlen=512 ./qemu_eq_test

# Clean — add qemu_eq_test to existing clean target
# Replace your current clean rule with this one:
# clean:
#     rm -f $(TARGET) qemu_eq_test output.raw
