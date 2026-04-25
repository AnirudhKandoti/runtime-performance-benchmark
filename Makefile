CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
TARGET := runtime_benchmark
SRC := src/main.cpp

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET) --sizes 10000,50000,100000 --iterations 5 --out-dir results

clean:
	rm -f $(TARGET)
	rm -rf build results results-test
