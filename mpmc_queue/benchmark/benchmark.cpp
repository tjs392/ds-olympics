#include "mpmc_queue.hpp"
#include <boost/lockfree/queue.hpp>
#include <queue>
#include <mutex>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <iostream>

using namespace ds_olympics;

template <typename Queue>
double benchmark_queue(int num_producers, int num_consumers, int items_per_producer, size_t& total_bytes) {
    Queue q(num_producers * items_per_producer);
    std::atomic<size_t> consumed_count{0};
    size_t total_items = num_producers * items_per_producer;
    total_bytes = total_items * sizeof(int);

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&q, p, items_per_producer]() {
            for (int i = 0; i < items_per_producer; ++i) {
                while (!q.push(p * items_per_producer + i)) std::this_thread::yield();
            }
        });
    }

    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&q, &consumed_count, total_items]() {
            while (consumed_count.load() < total_items) {
                auto val = q.pop();
                if (val.has_value()) consumed_count.fetch_add(1, std::memory_order_relaxed);
                else std::this_thread::yield();
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    double duration_us = std::chrono::duration<double, std::micro>(end - start).count();
    return duration_us;
}

template <>
double benchmark_queue<std::queue<int>>(int num_producers, int num_consumers, int items_per_producer, size_t& total_bytes) {
    std::queue<int> q;
    std::mutex mtx;
    std::atomic<size_t> consumed_count{0};
    size_t total_items = num_producers * items_per_producer;
    total_bytes = total_items * sizeof(int);

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&q, &mtx, p, items_per_producer]() {
            for (int i = 0; i < items_per_producer; ++i) {
                std::lock_guard<std::mutex> lock(mtx);
                q.push(p * items_per_producer + i);
            }
        });
    }

    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&q, &mtx, &consumed_count, total_items]() {
            while (consumed_count.load() < total_items) {
                std::lock_guard<std::mutex> lock(mtx);
                if (!q.empty()) {
                    q.pop();
                    consumed_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    double duration_us = std::chrono::duration<double, std::micro>(end - start).count();
    return duration_us;
}

template <>
double benchmark_queue<boost::lockfree::queue<int>>(int num_producers, int num_consumers, int items_per_producer, size_t& total_bytes) {
    boost::lockfree::queue<int> q(num_producers * items_per_producer);
    std::atomic<size_t> consumed_count{0};
    size_t total_items = num_producers * items_per_producer;
    total_bytes = total_items * sizeof(int);

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&q, p, items_per_producer]() {
            for (int i = 0; i < items_per_producer; ++i) {
                while (!q.push(p * items_per_producer + i)) std::this_thread::yield();
            }
        });
    }

    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&q, &consumed_count, total_items]() {
            int val;
            while (consumed_count.load() < total_items) {
                if (q.pop(val)) consumed_count.fetch_add(1, std::memory_order_relaxed);
                else std::this_thread::yield();
            }
        });
    }

    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    auto end = std::chrono::high_resolution_clock::now();
    double duration_us = std::chrono::duration<double, std::micro>(end - start).count();
    return duration_us;
}

int main() {
    int num_producers = 4;
    int num_consumers = 4;
    int items_per_producer = 100000;

    size_t total_bytes;
    double time_us;

    time_us = benchmark_queue<MPMCQueue<int>>(num_producers, num_consumers, items_per_producer, total_bytes);
    std::cout << "MPMCQueue<int> benchmark:\n"
              << "  Producers: " << num_producers << "\n"
              << "  Consumers: " << num_consumers << "\n"
              << "  Total items: " << num_producers * items_per_producer << "\n"
              << "  Total bytes: " << total_bytes << " bytes\n"
              << "  Total time: " << time_us << " microseconds\n"
              << "  Throughput: " << (total_bytes / time_us) << " MB/s\n\n";

    time_us = benchmark_queue<std::queue<int>>(num_producers, num_consumers, items_per_producer, total_bytes);
    std::cout << "std::queue<int> + mutex benchmark:\n"
              << "  Producers: " << num_producers << "\n"
              << "  Consumers: " << num_consumers << "\n"
              << "  Total items: " << num_producers * items_per_producer << "\n"
              << "  Total bytes: " << total_bytes << " bytes\n"
              << "  Total time: " << time_us << " microseconds\n"
              << "  Throughput: " << (total_bytes / time_us) << " MB/s\n\n";

    time_us = benchmark_queue<boost::lockfree::queue<int>>(num_producers, num_consumers, items_per_producer, total_bytes);
    std::cout << "boost::lockfree::queue<int> benchmark:\n"
              << "  Producers: " << num_producers << "\n"
              << "  Consumers: " << num_consumers << "\n"
              << "  Total items: " << num_producers * items_per_producer << "\n"
              << "  Total bytes: " << total_bytes << " bytes\n"
              << "  Total time: " << time_us << " microseconds\n"
              << "  Throughput: " << (total_bytes / time_us) << " MB/s\n";

    return 0;
}
