CXX = g++
CXXFLAGS = -std=c++17

TEST_SRC = tests/test_pipeline.cpp \
           src/pipeline.cpp \
           src/gaussian.cpp \
           src/sobel.cpp

TARGET = runTests

build:
	$(CXX) $(CXXFLAGS) $(TEST_SRC) -lgtest -lgtest_main -pthread -o $(TARGET)

run:
	./$(TARGET)

test: build run
