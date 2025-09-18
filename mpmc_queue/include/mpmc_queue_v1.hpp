#pragma once

#include <atomic>
#include <vector>
#include <thread>
#include <immintrin.h>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <optional>

/*
 * High-Performance MPMC Queue for small objects
 * Optimized for AMD Ryzen AI 7 PRO 350 (8 cores / 16 threads)
 * - 64-byte cache lines
 * - Low-latency multi-producer / multi-consumer workloads
 */

namespace mpmc_queue {

constexpr size_t CACHE_LINE_SIZE = 64;

template <typename T>
class MPMCQueue {
private:
    struct alignas(CACHE_LINE_SIZE) Slot {
        std::atomic<size_t> seq;
        char pad1[CACHE_LINE_SIZE - sizeof(std::atomic<size_t>)];
        T value;
        char pad2[CACHE_LINE_SIZE - sizeof(T) % CACHE_LINE_SIZE];
    };

    size_t capacity_;
    size_t mask_;
    std::vector<Slot> buffer_;

    alignas(64) std::atomic<size_t> head_;
    char head_pad_[CACHE_LINE_SIZE - sizeof(std::atomic<size_t>)] = {};

    alignas(64) std::atomic<size_t> tail_;
    char tail_pad_[CACHE_LINE_SIZE - sizeof(std::atomic<size_t>)] = {};


    static size_t round_up_pow2(size_t n) {
        size_t x = 1;
        while (x < n) x <<= 1;
        return x;
    }

public:
    explicit MPMCQueue(size_t capacity)
        : capacity_(round_up_pow2(capacity)),
          mask_(capacity_ - 1),
          buffer_(capacity_),
          head_(0),
          tail_(0)
    {
        assert((capacity_ & (capacity_ - 1)) == 0 && "Capacity must be power of2");
        for (size_t i = 0; i < capacity_; ++i) {
            buffer_[i].seq.store(i, std::memory_order_relaxed);
        }
    }

    ~MPMCQueue() = default;
    MPMCQueue(const MPMCQueue&) = delete;
    MPMCQueue& operator=(const MPMCQueue&) = delete;

    bool push(const T& item) {
        size_t tail = tail_.load(std::memory_order_relaxed);
        int spins = 0;

        while (true) {
            Slot& slot = buffer_[tail & mask_];
            size_t seq = slot.seq.load(std::memory_order_acquire);
            size_t diff = seq - tail;

            if (diff == 0) {
                if (tail_.compare_exchange_weak(
                        tail, tail + 1,
                        std::memory_order_acq_rel,
                        std::memory_order_relaxed
                    ))
                {
                    slot.value = item;
                    slot.seq.store(tail + 1, std::memory_order_release);

                    _mm_prefetch(reinterpret_cast<const char*>(&buffer_[(tail + 4) & mask_]), _MM_HINT_T0);

                    return true;
                }
                spins = 0;
            } else if (diff > capacity_) {
                return false;
            } else {
                tail = tail_.load(std::memory_order_relaxed);
                if (++spins < 20) _mm_pause();
                else if (spins < 100) _mm_pause();
                else if (spins < 1000) std::this_thread::yield();
                else std::this_thread::sleep_for(std::chrono::nanoseconds(1));
            }
        }
    }

    bool pop(T& out) {
        size_t head = head_.load(std::memory_order_relaxed);
        int spins = 0;

        while (true) {
            Slot& slot = buffer_[head & mask_];
            size_t seq = slot.seq.load(std::memory_order_acquire);
            size_t diff = seq - (head + 1);

            if (diff == 0) {
                if (head_.compare_exchange_weak(
                        head, head + 1,
                        std::memory_order_acq_rel,
                        std::memory_order_relaxed
                    ))
                {
                    out = slot.value;
                    slot.seq.store(head + capacity_, std::memory_order_release);

                    _mm_prefetch(reinterpret_cast<const char*>(&buffer_[(head + 4) & mask_]), _MM_HINT_T0);

                    return true;
                }
                spins = 0;
            } else if (diff > capacity_) {
                return false;
            } else {
                head = head_.load(std::memory_order_relaxed);
                if (++spins < 20) _mm_pause();
                else if (spins < 100) _mm_pause();
                else if (spins < 1000) std::this_thread::yield();
                else std::this_thread::sleep_for(std::chrono::nanoseconds(1));
            }
        }
    }
};

}