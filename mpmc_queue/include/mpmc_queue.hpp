#pragma once

#include <cstddef>
#include <vector>
#include <optional>
#include <atomic>
#include <thread>
#include <chrono>
#include <immintrin.h>

namespace ds_olympics {

template <typename T>
class MPMCQueue {
private:
    struct Slot {
        std::atomic<size_t> seq;
        T value;
    };

    const size_t capacity_;
    std::vector<Slot> buffer_;
    std::atomic<size_t> head_;
    std::atomic<size_t> tail_;

public:
    explicit MPMCQueue(size_t capacity)
        :   capacity_(capacity),
            buffer_(capacity),
            head_(0),
            tail_(0)
    {
        for (size_t i = 0; i < capacity_; i++) {
            buffer_[i].seq.store(i, std::memory_order_relaxed);
        }
    }

    ~MPMCQueue()  =default;

    MPMCQueue(const MPMCQueue&) = delete;
    MPMCQueue& operator=(const MPMCQueue&) = delete;

    bool push(const T& item) {
        Slot* slot;
        size_t tail = tail_.load(std::memory_order_relaxed);

        int spins = 0;

        while (true) {
            slot = &buffer_[tail % capacity_];
            size_t seq = slot->seq.load(std::memory_order_acquire);
            intptr_t diff = intptr_t(seq) - (intptr_t)tail;

            if (diff == 0) {
                if (tail_.compare_exchange_weak(
                        tail, tail + 1,
                        std::memory_order_acq_rel,
                        std::memory_order_relaxed
                    ))
                {
                    slot->value = item;
                    slot->seq.store(tail + 1, std::memory_order_release);
                    return true;
                }
                spins = 0;
                
            } else if (diff < 0) {
                return false;

            } else {
                tail = tail_.load(std::memory_order_relaxed);

                if (++spins < 10) {
                    _mm_pause();

                } else if (spins < 100) {
                    std::this_thread::yield();

                } else {
                    std::this_thread::sleep_for(std::chrono::nanoseconds(50));
                }
            }
        }
    }

    std::optional<T> pop() {
        Slot* slot;
        size_t head = head_.load(std::memory_order_relaxed);

        int spins = 0;

        while (true) {
            slot = &buffer_[head % capacity_];
            size_t seq = slot->seq.load(std::memory_order_acquire);
            intptr_t diff = (intptr_t)seq - (intptr_t)(head + 1);

            if (diff == 0) {
                if (head_.compare_exchange_weak(
                        head, head + 1,
                        std::memory_order_acq_rel,
                        std::memory_order_relaxed
                    ))
                {
                    T value = slot->value;
                    slot->seq.store(head + capacity_, std::memory_order_release);
                    return value;
                }
                spins = 0;

            } else if (diff < 0) {
                return std::nullopt;

            } else {
                head = head_.load(std::memory_order_relaxed);

                if (++spins < 10) {
                    _mm_pause();

                } else if (spins < 100) {
                    std::this_thread::yield();

                } else {
                    std::this_thread::sleep_for(std::chrono::nanoseconds(50));
                }
            }
        }
    }
};

}