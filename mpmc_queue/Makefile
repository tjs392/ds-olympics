CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -Iinclude -pthread
LDFLAGS = -lgtest -lgtest_main -pthread

SRC_TEST = tests/mpmc_tests.cpp
TARGET_TEST = run_tests

SRC_BENCH = benchmark/benchmark.cpp
TARGET_BENCH = run_benchmark

all: $(TARGET_TEST)

$(TARGET_TEST): $(SRC_TEST)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(TARGET_BENCH): $(SRC_BENCH)
	$(CXX) $(CXXFLAGS) $^ -o $@ 

test: $(TARGET_TEST)
	./$(TARGET_TEST)

benchmark: $(TARGET_BENCH)
	./$(TARGET_BENCH)

clean:
	rm -f $(TARGET_TEST) $(TARGET_BENCH)
