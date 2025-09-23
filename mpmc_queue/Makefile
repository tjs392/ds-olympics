CXX = g++
CXXFLAGS = -std=c++23 -Wall -Wextra -Iinclude -Iexternal -pthread
LDFLAGS = -lgtest -lgtest_main -pthread

SRC_TEST = tests/mpmc_tests.cpp
TARGET_TEST = run_tests

SRC_BENCH = benchmark/benchmark.cpp
TARGET_BENCH = run_benchmark

SRC_SINGLE = benchmark/single.cpp
TARGET_SINGLE = run_single

all: $(TARGET_TEST)

$(TARGET_TEST): $(SRC_TEST)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(TARGET_BENCH): $(SRC_BENCH)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(TARGET_SINGLE): $(SRC_SINGLE)
	$(CXX) $(CXXFLAGS) $^ -o $@

test: $(TARGET_TEST)
	./$(TARGET_TEST)

benchmark: $(TARGET_BENCH)
	./$(TARGET_BENCH)

single: $(TARGET_SINGLE)
	./$(TARGET_SINGLE)

perf: $(TARGET_BENCH)
	../../wsl2-tools/WSL2-Linux-Kernel/tools/perf/perf stat \
		-e cache-misses,cache-references,cycles,instructions,branches,branch-misses \
		./$(TARGET_BENCH)

perf-single: $(TARGET_SINGLE)
	../../wsl2-tools/WSL2-Linux-Kernel/tools/perf/perf stat \
		-e cache-misses,cache-references,cycles,instructions,branches,branch-misses \
		./$(TARGET_SINGLE)

perf-detailed: $(TARGET_BENCH)
	../../wsl2-tools/WSL2-Linux-Kernel/tools/perf/perf record -e cache-misses:u ./$(TARGET_BENCH)

perf-report: $(TARGET_BENCH)
	../../wsl2-tools/WSL2-Linux-Kernel/tools/perf/perf report

clean:
	rm -f $(TARGET_TEST) $(TARGET_BENCH) $(TARGET_SINGLE) perf.data
