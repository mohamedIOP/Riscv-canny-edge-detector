CXX = riscv64-unknown-elf-g++
TARGET = canny
CXXFLAGS = -static -march=rv64gcv -mabi=lp64d -O3 -std=c++17 -I"Phase 2/include"

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) main.cpp \
		"Phase 2"/src/gaussian.cpp \
		"Phase 2"/src/sobel.cpp \
		"Phase 2"/src/magnitude.cpp \
		"Phase 2"/src/direction.cpp \
		-o $(TARGET)

# Run at all three VLEN values to verify vector-length-agnostic correctness
run: $(TARGET)
	@echo "=== VLEN=128 ==="
	qemu-riscv64 -cpu rv64,v=true,vlen=128 ./$(TARGET) Input_Images/input.raw 256 256 Output_Images/output_128.raw
	@echo "=== VLEN=256 ==="
	qemu-riscv64 -cpu rv64,v=true,vlen=256 ./$(TARGET) Input_Images/input.raw 256 256 Output_Images/output_256.raw
	@echo "=== VLEN=512 ==="
	qemu-riscv64 -cpu rv64,v=true,vlen=512 ./$(TARGET) Input_Images/input.raw 256 256 Output_Images/output_512.raw

clean:
	rm -f $(TARGET) runTests visual_pipeline qemu_eq_test
	rm -f Output_Images/*.raw
	rm -f output_gaussian.raw output_sobel_gx.raw output_sobel_gy.raw \
	      output_magnitude_l1.raw output_direction.raw \
	      output_128.raw output_256.raw output_512.raw

test:
	g++ -std=c++17 \
	-I"Phase 2/include" \
	tests/test_pipeline.cpp \
	src/pipeline.cpp \
	"Phase 2"/src/gaussian.cpp \
	"Phase 2"/src/sobel.cpp \
	"Phase 2"/src/magnitude.cpp \
	"Phase 2"/src/direction.cpp \
	-lgtest -lgtest_main -pthread \
	-o runTests
	./runTests
visual:
	g++ -std=c++17 -I"Phase 2/include" -I"Phase 2/src" visual_pipeline.cpp \
	"Phase 2"/src/gaussian.cpp \
	"Phase 2"/src/sobel.cpp \
	"Phase 2"/src/magnitude.cpp \
	"Phase 2"/src/direction.cpp \
	-o visual_pipeline

# ── Student C: QEMU-side equivalence tests ──────────────────
RV_CXX   = riscv64-unknown-elf-g++
RV_FLAGS = -static -march=rv64gcv -mabi=lp64d -O2 -std=c++17
QEMU     = qemu-riscv64
QEMU_CPU = rv64,v=true

qemu_eq_test:
	$(RV_CXX) $(RV_FLAGS) \
		-I"Phase 2/include" \
		tests/qemu_equivalence_test.cpp \
		"Phase 2"/src/gaussian.cpp \
		"Phase 2"/src/sobel.cpp \
		"Phase 2"/src/magnitude.cpp \
		"Phase 2"/src/direction.cpp \
		-o qemu_eq_test

test_qemu: qemu_eq_test
	@echo "--- VLEN=128 ---"
	$(QEMU) -cpu $(QEMU_CPU),vlen=128 ./qemu_eq_test
	@echo "--- VLEN=256 ---"
	$(QEMU) -cpu $(QEMU_CPU),vlen=256 ./qemu_eq_test
	@echo "--- VLEN=512 ---"
	$(QEMU) -cpu $(QEMU_CPU),vlen=512 ./qemu_eq_test
