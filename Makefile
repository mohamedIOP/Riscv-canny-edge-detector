CXX = riscv64-unknown-elf-g++
TARGET = canny
CXXFLAGS = -static -march=rv64gcv -mabi=lp64d -O3 -std=c++17

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o $(TARGET)

run: $(TARGET)
	qemu-riscv64 -cpu rv64,v=true,vlen=128 ./$(TARGET) input.raw 256 256 output.raw

clean:
	rm -f $(TARGET) output.rawg
run_tests:
	g++ -std=c++14 "Phase 2/src/gaussian.cpp" "Phase 2/src/sobel.cpp" src/pipeline.cpp tests/test_pipeline.cpp -I"Phase 2/include" -o pipeline_tests -lgtest -lgtest_main -lpthread
	./pipeline_tests
