CXX = riscv64-unknown-elf-g++
TARGET = canny
CXXFLAGS = -static -march=rv64gcv -mabi=lp64d -O3 -std=c++17 -I"Phase 2/include"

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o $(TARGET)

run: $(TARGET)
	qemu-riscv64 -cpu rv64,v=true,vlen=128 ./$(TARGET) input.raw 256 256 output.raw

clean:
	rm -f $(TARGET) output.raw

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
	./visual_pipeline

