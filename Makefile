# Define the compiler and the flags
CXX = riscv64-linux-gnu-g++
CXXFLAGS = -static -march=rv64gcv -mabi=lp64d -O3

# Define the target executable and source file
TARGET = canny
SRC = main.cpp

# Default rule
all: $(TARGET)

# Rule to compile the C++ code
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

# Rule to run the program through QEMU
run: $(TARGET)
	qemu-riscv64 -cpu rv64,v=true,vlen=128 ./$(TARGET)

# Rule to clean up the directory
clean:
	rm -f $(TARGET) output.raw

