#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

#include "../src/types.h"

namespace {

using Clock = std::chrono::steady_clock;

constexpr uint64 Iterations = 10'000'000;
constexpr usize CacheLineSize = 64;
constexpr int Repetitions = 5;

// ------------------------------------------------------------
// Case 1: Potential false sharing
// ------------------------------------------------------------
//
// Both counters are adjacent in memory.
//
// Each thread writes a different counter, but the counters
// may occupy the same CPU cache line.

struct PackedCounters {
  std::atomic<uint64> first{0};
  std::atomic<uint64> second{0};
};

// ------------------------------------------------------------
// Case 2: Separate cache lines
// ------------------------------------------------------------
//
// alignas(64) places each counter at a 64-byte boundary.
//
// 64 bytes is a common cache-line size, but not a universal
// hardware guarantee.

struct AlignedCounters {
  alignas(CacheLineSize) std::atomic<uint64> first{0};
  alignas(CacheLineSize) std::atomic<uint64> second{0};
};

static_assert(alignof(AlignedCounters) >= CacheLineSize);

// ------------------------------------------------------------
// Benchmark
// ------------------------------------------------------------

template <typename Counters>
real64 RunBenchmark() {
  Counters counters;

  // Both workers wait until the main thread releases them.
  // This avoids counting thread creation in the measurement.
  std::atomic<bool32> start{false};

  auto worker = [&](std::atomic<uint64>& counter) {
    while (!start.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }

    for (uint64 i = 0; i < Iterations; ++i) {
      // Each thread exclusively modifies its own counter.
      // No ordering with the other counter is required.
      counter.fetch_add(1, std::memory_order_relaxed);
    }
  };

  std::thread first_thread(worker, std::ref(counters.first));
  std::thread second_thread(worker, std::ref(counters.second));

  const auto start_time = Clock::now();

  start.store(true, std::memory_order_release);

  first_thread.join();
  second_thread.join();

  const auto end_time = Clock::now();

  // Both threads have joined, so reading the final values
  // requires no additional inter-thread synchronization.
  const uint64 first = counters.first.load(std::memory_order_relaxed);

  const uint64 second = counters.second.load(std::memory_order_relaxed);

  if (first != Iterations || second != Iterations) {
    std::printf("ERROR: incorrect counter values\n");
    std::abort();
  }

  return std::chrono::duration<real64>(end_time - start_time).count();
}

template <typename Counters>
real64 RunRepeatedBenchmark(const char* name) {
  real64 total = 0;

  for (int i = 0; i < Repetitions; ++i) {
    const real64 seconds = RunBenchmark<Counters>();

    std::printf("  run %d: %.6f s\n", i + 1, seconds);

    total += seconds;
  }

  const real64 average = total / Repetitions;

  std::printf("  average: %.6f s\n\n", average);

  return average;
}

}  // namespace

int main() {
  std::printf(
      "False Sharing Benchmark\n"
      "=======================\n\n");

  std::printf("Iterations per thread: %llu\n",
              static_cast<unsigned long long>(Iterations));

  std::printf("Repetitions: %d\n\n", Repetitions);

  std::printf("Packed counters:\n");
  const real64 packed = RunRepeatedBenchmark<PackedCounters>("Packed");

  std::printf("Aligned counters:\n");
  const real64 aligned = RunRepeatedBenchmark<AlignedCounters>("Aligned");

  std::printf(
      "Comparison\n"
      "==========\n"
      "Packed average:  %.6f s\n"
      "Aligned average: %.6f s\n"
      "Speedup:         %.3fx\n",
      packed,
      aligned,
      packed / aligned);

  return 0;
}
