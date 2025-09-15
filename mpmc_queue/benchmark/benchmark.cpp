#include "mpmc_queue.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <tuple>

using namespace mpmc_queue;

struct SmallObject {
    int i;
    double d;
    float f;
};

template <typename T>
void benchmark_type(const std::string& type_name, int num_producers, int num_consumers, int items_per_producer) {
    size_t total_items = num_producers * items_per_producer;
    size_t total_bytes = total_items * sizeof(T);

    MPMCQueue<T> q(total_items);
    std::atomic<size_t> consumed_count{0};

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&q, p, items_per_producer]() {
            for (int i = 0; i < items_per_producer; ++i) {
                T value{};
                if constexpr (std::is_same_v<T, SmallObject>) {
                    value.i = i;
                    value.d = i * 0.5;
                    value.f = i * 0.25f;
                } else {
                    value = static_cast<T>(i + p * items_per_producer);
                }
                while (!q.push(value)) std::this_thread::yield();
            }
        });
    }

    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&q, &consumed_count, total_items]() {
            while (consumed_count.load(std::memory_order_relaxed) < total_items) {
                T val;
                if (q.pop(val)) {
                    consumed_count.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }

    for (auto &t : producers) t.join();
    for (auto &t : consumers) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    double duration_s = std::chrono::duration<double>(end - start).count();

    double ops_per_sec = total_items / duration_s / 1e6;
    double mb_per_sec = (total_bytes / 1024.0 / 1024.0) / duration_s;
    double ns_per_op = duration_s * 1e9 / total_items;

    std::cout << "==== " << num_producers << "P / " << num_consumers << "C | " 
              << type_name << " ====\n";
    std::cout << "  Total items: " << total_items << "\n";
    std::cout << "  Time: " << duration_s << " s\n";
    std::cout << "  Throughput: " << std::fixed << std::setprecision(4) << ops_per_sec << " million ops/sec\n";
    std::cout << "  Throughput: " << mb_per_sec << " MB/s\n";
    std::cout << "  Avg latency: " << ns_per_op << " ns/op\n\n";
}

int main() {
    const int items_per_producer = 1000000;

    const int max_threads = std::thread::hardware_concurrency() - 2;
    std::cout << "Detected CPU max threads: " << max_threads << "\n\n";

    std::vector<std::tuple<int,int>> thread_configs = {
        // {1,1}, {8,8}, 
        {max_threads,max_threads}, 
        {1,max_threads}, {max_threads,1}
        //, {4,1}, {1,4}, {2,4}, {4,2}
    };

    for (auto &[p,c] : thread_configs) {
        benchmark_type<int>("int", p, c, items_per_producer);
        // benchmark_type<double>("double", p, c, items_per_producer);
        // benchmark_type<SmallObject>("SmallObject", p, c, items_per_producer);
    }

    return 0;
}
