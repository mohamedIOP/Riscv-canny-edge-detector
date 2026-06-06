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
	rm -f $(TARGET) "Output_Images/output__128.raw" "Output_Images/output__256.raw" "Output_Images/output__512.raw"

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
