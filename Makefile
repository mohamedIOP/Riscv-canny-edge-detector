CXX = riscv64-unknown-elf-g++
TARGET = canny
CXXFLAGS = -static -march=rv64gcv -mabi=lp64d -O3 -std=c++17

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o $(TARGET)

run: $(TARGET)
	qemu-riscv64 -cpu rv64,v=true,vlen=128 ./$(TARGET) input.raw 256 256 output.raw

clean:
	rm -f $(TARGET) output.raw
test:
	g++ -std=c++17 \
	    tests/test_pipeline.cpp \
	    src/pipeline.cpp \
	    src/gaussian.cpp \
	    src/sobel.cpp \
	    -lgtest -lgtest_main -pthread \
	    -o runTests
