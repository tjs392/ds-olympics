#include "mpmc_queue.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <string>
#include <utility>
#include <numeric>

using namespace mpmc_queue;

struct alignas(64) ThreadStats {
    size_t ops = 0;
    size_t dummy = 0;
};

struct SmallObject {
    int i;
    double d;
    float f;
};

void pin_thread(int core_id) {
#ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
}

template <typename T>
void benchmark_mpmc(const std::string& name, int num_producers, int num_consumers, size_t items_per_producer) {
    const size_t total_items = num_producers * items_per_producer;
    MPMCQueue<T> q(total_items);

    std::vector<ThreadStats> producer_stats(num_producers);
    std::vector<ThreadStats> consumer_stats(num_consumers);

    std::atomic<bool> start_flag{false};

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&, p]() {
            pin_thread(p);
            while (!start_flag.load(std::memory_order_acquire)) _mm_pause();
            for (size_t i = 0; i < items_per_producer; ++i) {
                T val{};
                if constexpr (std::is_same_v<T, SmallObject>) {
                    val.i = i;
                    val.d = i * 0.5;
                    val.f = i * 0.25f;
                } else {
                    val = static_cast<T>(i + p * items_per_producer);
                }
                while (!q.push(val)) _mm_pause();
                producer_stats[p].ops++;
            }
        });
    }

    std::atomic<size_t> consumed_total{0};
    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&, c]() {
            pin_thread(num_producers + c);
            while (!start_flag.load(std::memory_order_acquire)) _mm_pause();
            ThreadStats& stats = consumer_stats[c];
            while (consumed_total.load(std::memory_order_relaxed) < total_items) {
                T val;
                if (q.pop(val)) {
                    stats.ops++;
                    consumed_total.fetch_add(1, std::memory_order_relaxed);
                    if constexpr (std::is_same_v<T, SmallObject>) {
                        stats.dummy += val.i + static_cast<size_t>(val.d) + static_cast<size_t>(val.f);
                    } else {
                        stats.dummy += static_cast<size_t>(val);
                    }
                } else {
                    _mm_pause();
                }
            }
        });
    }

    auto start = std::chrono::high_resolution_clock::now();
    start_flag.store(true, std::memory_order_release);

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();
    auto end = std::chrono::high_resolution_clock::now();

    double duration_s = std::chrono::duration<double>(end - start).count();
    size_t total_ops = std::accumulate(producer_stats.begin(), producer_stats.end(), 0ull,
                                       [](size_t sum, const ThreadStats& s){ return sum + s.ops; }) +
                       std::accumulate(consumer_stats.begin(), consumer_stats.end(), 0ull,
                                       [](size_t sum, const ThreadStats& s){ return sum + s.ops; });

    size_t total_dummy = std::accumulate(consumer_stats.begin(), consumer_stats.end(), 0ull,
                                         [](size_t sum, const ThreadStats& s){ return sum + s.dummy; });

    double ops_per_sec = total_ops / duration_s / 1e6;
    double ns_per_op = duration_s * 1e9 / total_items;

    std::cout << "==== " << num_producers << "P / " << num_consumers << "C | " << name << " ====\n";
    std::cout << "  Total items: " << total_items << "\n";
    std::cout << "  Time: " << duration_s << " s\n";
    std::cout << "  Throughput: " << std::fixed << std::setprecision(4) << ops_per_sec << " M ops/sec\n";
    std::cout << "  Avg latency: " << ns_per_op << " ns/op\n";
    std::cout << "  Dummy sum: " << total_dummy << " (prevents optimization)\n\n";
}

int main() {
    const size_t items_per_producer = 1'000'000;
    const int max_threads = std::thread::hardware_concurrency();
    
    std::vector<std::pair<int,int>> configs = {
        {1,1},
        {max_threads/2, max_threads/2},
        {max_threads, max_threads}
    };

    for (auto& [p,c] : configs) {
        benchmark_mpmc<int>("int", p, c, items_per_producer);
        benchmark_mpmc<SmallObject>("SmallObject", p, c, items_per_producer);
    }

    return 0;
}
