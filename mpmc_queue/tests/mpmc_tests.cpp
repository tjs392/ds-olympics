#include "mpmc_queue.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_set>

using namespace mpmc_queue;

TEST(MPMCQueueTest, BasicPushPop) {
    MPMCQueue<int> q(4);
    EXPECT_TRUE(q.push(10));
    EXPECT_TRUE(q.push(20));
    auto val1 = q.pop();
    ASSERT_TRUE(val1.has_value());
    EXPECT_EQ(val1.value(), 10);
    auto val2 = q.pop();
    ASSERT_TRUE(val2.has_value());
    EXPECT_EQ(val2.value(), 20);
}

TEST(MPMCQueueTest, FullQueue) {
    MPMCQueue<int> q(2);
    EXPECT_TRUE(q.push(1));
    EXPECT_TRUE(q.push(2));
    EXPECT_FALSE(q.push(3));
}

TEST(MPMCQueueTest, EmptyQueue) {
    MPMCQueue<int> q(2);
    EXPECT_FALSE(q.pop().has_value());
}

TEST(MPMCQueueTest, MultipleProducersSingleConsumer) {
    const int num_producers = 4;
    const int items_per_producer = 1000;
    MPMCQueue<int> q(num_producers * items_per_producer);
    std::vector<int> results;
    std::mutex results_mutex;
    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([p, &q, items_per_producer]() {
            for (int i = 0; i < items_per_producer; ++i) {
                while (!q.push(p * items_per_producer + i)) std::this_thread::yield();
            }
        });
    }
    std::thread consumer([&]() {
        int total_items = num_producers * items_per_producer;
        int consumed = 0;
        while (consumed < total_items) {
            auto val = q.pop();
            if (val.has_value()) {
                std::lock_guard<std::mutex> lock(results_mutex);
                results.push_back(val.value());
                ++consumed;
            } else {
                std::this_thread::yield();
            }
        }
    });
    for (auto &t : producers) t.join();
    consumer.join();
    EXPECT_EQ(results.size(), num_producers * items_per_producer);
    std::unordered_set<int> unique(results.begin(), results.end());
    EXPECT_EQ(unique.size(), results.size());
}

TEST(MPMCQueueTest, SingleProducerMultipleConsumers) {
    const int num_consumers = 4;
    const int total_items = 1000;
    MPMCQueue<int> q(total_items);
    std::vector<int> results;
    std::mutex results_mutex;
    std::thread producer([&]() {
        for (int i = 0; i < total_items; ++i) {
            while (!q.push(i)) std::this_thread::yield();
        }
    });
    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&]() {
            while (true) {
                auto val = q.pop();
                if (val.has_value()) {
                    std::lock_guard<std::mutex> lock(results_mutex);
                    results.push_back(val.value());
                } else if (results.size() >= total_items) {
                    break;
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }
    producer.join();
    for (auto &t : consumers) t.join();
    EXPECT_EQ(results.size(), total_items);
    std::unordered_set<int> unique(results.begin(), results.end());
    EXPECT_EQ(unique.size(), results.size());
}

TEST(MPMCQueueTest, MultipleProducersMultipleConsumers) {
    const int num_producers = 4;
    const int num_consumers = 4;
    const int items_per_producer = 500;
    MPMCQueue<int> q(num_producers * items_per_producer);
    std::vector<int> results;
    std::mutex results_mutex;
    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([p, &q, items_per_producer]() {
            for (int i = 0; i < items_per_producer; ++i) {
                while (!q.push(p * items_per_producer + i)) std::this_thread::yield();
            }
        });
    }
    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&]() {
            while (true) {
                auto val = q.pop();
                if (val.has_value()) {
                    std::lock_guard<std::mutex> lock(results_mutex);
                    results.push_back(val.value());
                } else if (results.size() >= num_producers * items_per_producer) {
                    break;
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }
    for (auto &t : producers) t.join();
    for (auto &t : consumers) t.join();
    EXPECT_EQ(results.size(), num_producers * items_per_producer);
    std::unordered_set<int> unique(results.begin(), results.end());
    EXPECT_EQ(unique.size(), results.size());
}

TEST(MPMCQueueTest, HighContentionStress) {
    const int num_producers = 8;
    const int num_consumers = 8;
    const int items_per_producer = 10000;
    MPMCQueue<int> q(num_producers * items_per_producer);
    std::vector<int> results;
    std::mutex results_mutex;
    std::vector<std::thread> producers;
    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([p, &q, items_per_producer]() {
            for (int i = 0; i < items_per_producer; ++i) {
                while (!q.push(p * items_per_producer + i)) std::this_thread::yield();
            }
        });
    }
    std::vector<std::thread> consumers;
    for (int c = 0; c < num_consumers; ++c) {
        consumers.emplace_back([&]() {
            while (true) {
                auto val = q.pop();
                if (val.has_value()) {
                    std::lock_guard<std::mutex> lock(results_mutex);
                    results.push_back(val.value());
                } else if (results.size() >= num_producers * items_per_producer) {
                    break;
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }
    for (auto &t : producers) t.join();
    for (auto &t : consumers) t.join();
    EXPECT_EQ(results.size(), num_producers * items_per_producer);
    std::unordered_set<int> unique(results.begin(), results.end());
    EXPECT_EQ(unique.size(), results.size());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
