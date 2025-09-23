#pragma once
#include <vector>
#include <cstddef>
#include <cmath>
#include <cassert>

template <typename T>
struct alignas(64) PaddedT {
    T value;
    char padding[64 - sizeof(T) % 64];
};

struct alignas(64) PaddedIndex {
    size_t value = 0;
};

template <typename T>
class CircularQueue {
    size_t capacity_;
    std::vector<PaddedT<T>> buffer_;

    PaddedIndex head_;
    PaddedIndex tail_;

public:
    explicit CircularQueue(size_t capacity)
        : capacity_(1ULL << static_cast<size_t>(std::ceil(std::log2(capacity))))
        , buffer_(capacity_) {}

    bool push(const T& item) {
        size_t tail = tail_.value;
        size_t next = (tail + 1) & (capacity_ - 1);

        if (next == head_.value) return false;

        size_t prefetch_idx = (tail + 4) & (capacity_ - 1);
        __builtin_prefetch(&buffer_[prefetch_idx], 1, 3);

        buffer_[tail].value = item;
        tail_.value = next;
        return true;
    }

    bool pop(T& out) {
        size_t head = head_.value;
        if (head == tail_.value) return false;

        size_t prefetch_idx = (head + 4) & (capacity_ - 1);
        __builtin_prefetch(&buffer_[prefetch_idx], 0, 3);

        out = buffer_[head].value;
        head_.value = (head + 1) & (capacity_ - 1);
        return true;
    }
};
