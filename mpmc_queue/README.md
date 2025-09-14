# MPMCQueue (WIP)

## What this is
This folder contains a high-performance **multi-producer, multi-consumer queue** implementation in C++. It's designed for small, simple objects and uses lock-free techniques for low-latency access.

## Goals
- Build a fast, memory-efficient queue for concurrent access.
- Compare performance against:
  - `std::queue` (with `std::mutex`)
  - `boost::lockfree::queue`
- Experiment with different spinning and yielding strategies to see what gives the best throughput.

## What I want to see
I want to understand how this queue performs in practice and identify whether my lock-free implementation gives a real advantage over standard queues in concurrent workloads.
