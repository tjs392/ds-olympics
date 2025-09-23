#include <iostream>
#include <chrono>
#include "../include/single.hpp"

int main() {
    using Clock = std::chrono::high_resolution_clock;

    constexpr size_t NUM_ITEMS = 10'000'000;
    CircularQueue<int> q(1 << 16);

    for (int i = 0; i < 1000; i++) {
        q.push(i);
        int tmp;
        q.pop(tmp);
    }

    auto start = Clock::now();

    long long sum = 0;
    for (size_t i = 0; i < NUM_ITEMS; i++) {
        q.push(static_cast<int>(i));
        int out;
        q.pop(out);
        sum += out;
    }

    auto end = Clock::now();
    std::chrono::duration<double> elapsed = end - start;

    double throughput = NUM_ITEMS / elapsed.count();
    double latency_ns = (elapsed.count() / NUM_ITEMS) * 1e9;

    std::cout << "==== Single-thread CircularQueue<int> ====\n";
    std::cout << "Total items: " << NUM_ITEMS << "\n";
    std::cout << "Time: " << elapsed.count() << " s\n";
    std::cout << "Throughput: " << throughput / 1e6 << " M ops/sec\n";
    std::cout << "Avg latency: " << latency_ns << " ns/op\n";
    std::cout << "Dummy sum: " << sum << " (prevents optimization)\n";

    return 0;
}
