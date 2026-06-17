CXX = riscv64-unknown-elf-g++
TARGET = canny
CXXFLAGS = -static -march=rv64gcv -mabi=lp64d -O2 -std=c++17 -I"Phase 2/include"
# Scalar pipeline sources shared by every target (host + RISC-V).
# Bonus stages (nms, threshold) are pure scalar C++ and build everywhere.
SRCS = "Phase 2"/src/gaussian.cpp \
       "Phase 2"/src/sobel.cpp \
       "Phase 2"/src/magnitude.cpp \
       "Phase 2"/src/direction.cpp \
       "Phase 2"/src/nms.cpp \
       "Phase 2"/src/threshold.cpp

+QEMU = qemu-riscv64
+QEMU_CPU = rv64,v=true
+

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) main.cpp $(SRCS) -o $(TARGET)

run: $(TARGET)
	@echo "=== VLEN=128 ==="
	qemu-riscv64 -cpu rv64,v=true,vlen=128 ./$(TARGET) Input_Images/input.raw 256 256 Output_Images/output_128.raw
	@echo "=== VLEN=256 ==="
	qemu-riscv64 -cpu rv64,v=true,vlen=256 ./$(TARGET) Input_Images/input.raw 256 256 Output_Images/output_256.raw
	@echo "=== VLEN=512 ==="
	qemu-riscv64 -cpu rv64,v=true,vlen=512 ./$(TARGET) Input_Images/input.raw 256 256 Output_Images/output_512.raw

sweep:
	@echo "=== Building at all optimization levels ==="
	@echo "--- O0 ---"
	$(CXX) -static -march=rv64gcv -mabi=lp64d -O0 -std=c++17 \
		-I"Phase 2/include" main.cpp $(SRCS) -o canny_O0

	@echo "--- O2 ---"
	$(CXX) -static -march=rv64gcv -mabi=lp64d -O2 -std=c++17 \
		-I"Phase 2/include" main.cpp $(SRCS) -o canny_O2

	@echo "--- O3 ---"
	$(CXX) -static -march=rv64gcv -mabi=lp64d -O3 -std=c++17 \
		-I"Phase 2/include" main.cpp $(SRCS) -o canny_O3

	@echo "--- Os ---"
	$(CXX) -static -march=rv64gcv -mabi=lp64d -Os -std=c++17 \
		-I"Phase 2/include" main.cpp $(SRCS) -o canny_Os

	@echo "--- Ofast ---"
	$(CXX) -static -march=rv64gcv -mabi=lp64d -Ofast -std=c++17 \
	-I"Phase 2/include" main.cpp $(SRCS) -o canny_Ofast

	@echo "=== Binary sizes ==="
	ls -la canny_O0 canny_O2 canny_O3 canny_Os canny_Ofast

run_sweep: sweep
	@echo "=== Timing at O0 ==="
	$(QEMU) -cpu $(QEMU_CPU),vlen=256 ./canny_O0 \
		Input_Images/input.raw 256 256 Output_Images/output_O0.raw
	@echo "=== Timing at O2 ==="
	$(QEMU) -cpu $(QEMU_CPU),vlen=256 ./canny_O2 \
		Input_Images/input.raw 256 256 Output_Images/output_O2.raw
	@echo "=== Timing at O3 ==="
	$(QEMU) -cpu $(QEMU_CPU),vlen=256 ./canny_O3 \
		Input_Images/input.raw 256 256 Output_Images/output_O3.raw
	@echo "=== Timing at Os ==="
	$(QEMU) -cpu $(QEMU_CPU),vlen=256 ./canny_Os \
		Input_Images/input.raw 256 256 Output_Images/output_Os.raw
	@echo "=== Timing at Ofast ==="
	$(QEMU) -cpu $(QEMU_CPU),vlen=256 ./canny_Ofast \
		Input_Images/input.raw 256 256 Output_Images/output_Ofast.raw

clean:
	rm -f $(TARGET) runTests visual_pipeline qemu_eq_test
	rm -f canny_O0 canny_O2 canny_O3 canny_Os canny_Ofast canny_vec_report
	rm -f Output_Images/*.raw
	rm -f output_gaussian.raw output_sobel_gx.raw output_sobel_gy.raw \
	      output_magnitude_l1.raw output_direction.raw \
              output_nms.raw output_threshold.raw output_edges.raw \
	      output_128.raw output_256.raw output_512.raw

test:
	g++ -std=c++17 \
	-I"Phase 2/include" \
	tests/test_pipeline.cpp \
	src/pipeline.cpp $(SRCS) \
	-lgtest -lgtest_main -pthread \
	-o runTests
	./runTests

visual:
	g++ -std=c++17 -I"Phase 2/include" -I"Phase 2/src" visual_pipeline.cpp \
	$(SRCS) \
	-o visual_pipeline



qemu_eq_test:
	$(CXX) $(CXXFLAGS) \
		-I"Phase 2/include" \
		tests/qemu_equivalence_test.cpp $(SRCS) \
		-o qemu_eq_test

test_qemu: qemu_eq_test
	@echo "--- VLEN=128 ---"
	$(QEMU) -cpu $(QEMU_CPU),vlen=128 ./qemu_eq_test
	@echo "--- VLEN=256 ---"
	$(QEMU) -cpu $(QEMU_CPU),vlen=256 ./qemu_eq_test
	@echo "--- VLEN=512 ---"
	$(QEMU) -cpu $(QEMU_CPU),vlen=512 ./qemu_eq_test

separable:
	g++ -std=c++17 -O3 \
	-I"Phase 2/include" \
	"Phase 2/src/separable_experiment.cpp" \
	"Phase 2/src/gaussian.cpp" \
	-o /tmp/separable_exp && /tmp/separable_exp

prepad_experiment:
	g++ -std=c++17 -O3 -ftree-vectorize -fopt-info-vec-all \
	-I"Phase 2/include" \
	"Phase 2/src/prepad_experiment.cpp" \
	"Phase 2/src/gaussian.cpp" \
	-o /tmp/prepad_exp 2>prepad_vec_report.txt
	/tmp/prepad_exp
	@echo "--- Vectorized loops ---"
	@grep "optimized: loop vectorized" prepad_vec_report.txt || echo "(none)"

separable_autovec:
	g++ -std=c++17 -O3 -ftree-vectorize -fopt-info-vec-all \
	-I"Phase 2/include" \
	"Phase 2/src/separable_autovec_analysis.cpp" \
	"Phase 2/src/gaussian.cpp" \
	-o /tmp/sep_av 2>separable_vec_report.txt
	/tmp/sep_av
	@echo "--- Vectorized loops ---"
	@grep "optimized: loop vectorized" separable_vec_report.txt || echo "(none)"

autovec_investigation: prepad_experiment separable_autovec
	@echo "=== Investigation complete ==="
	@echo "See prepad_vec_report.txt and separable_vec_report.txt"	

