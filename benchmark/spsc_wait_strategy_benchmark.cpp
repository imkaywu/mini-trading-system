#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>

#include "../src/concurrency/spsc_queue.h"
#include "../src/types.h"

namespace {

using Clock = std::chrono::steady_clock;

constexpr uint64 ItemCount = 1'000'000;
constexpr usize QueueCapacity = 1024;
constexpr usize Repetitions = 5;

// ------------------------------------------------------------
// Waiting strategies
// ------------------------------------------------------------

enum class WaitStrategy {
  BusySpin,
  Yield,
  Hybrid,
};

const char* StrategyName(WaitStrategy strategy) {
  switch (strategy) {
    case WaitStrategy::BusySpin:
      return "Busy Spin";

    case WaitStrategy::Yield:
      return "Yield";

    case WaitStrategy::Hybrid:
      return "Hybrid";
  }

  return "Unknown";
}

// Called only after Push() or Pop() fails.
//
// BusySpin:
//   Immediately retry.
//
// Yield:
//   Give the OS scheduler an opportunity to run another thread.
//
// Hybrid:
//   Retry several times before yielding.
//
// This counter belongs exclusively to the calling thread.
void Wait(WaitStrategy strategy, uint32& failed_attempts) {
  ++failed_attempts;

  switch (strategy) {
    case WaitStrategy::BusySpin:
      // Intentionally do nothing.
      // The caller immediately retries.
      break;

    case WaitStrategy::Yield:
      std::this_thread::yield();
      break;

    case WaitStrategy::Hybrid:
      if (failed_attempts >= 100) {
        std::this_thread::yield();
        failed_attempts = 0;
      }
      break;
  }
}

// ------------------------------------------------------------
// Benchmark result
// ------------------------------------------------------------

struct BenchmarkResult {
  real64 seconds;
  real64 throughput;
};

// ------------------------------------------------------------
// One benchmark run
// ------------------------------------------------------------

BenchmarkResult RunBenchmark(WaitStrategy strategy) {
  concurrency::SpscQueue<uint64, QueueCapacity> queue;

  // Synchronize the beginning of the producer and consumer.
  std::atomic<bool32> start{false};

  uint64 consumed_count = 0;
  uint64 checksum = 0;

  std::thread consumer([&] {
    while (!start.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }

    uint64 value = 0;
    uint32 failed_attempts = 0;

    while (consumed_count < ItemCount) {
      if (queue.Pop(value)) {
        ++consumed_count;
        checksum += value;

        failed_attempts = 0;
      } else {
        // Queue is empty.
        Wait(strategy, failed_attempts);
      }
    }
  });

  uint32 failed_attempts = 0;

  const auto start_time = Clock::now();

  start.store(true, std::memory_order_release);

  // The main thread is the sole producer.
  for (uint64 i = 0; i < ItemCount; ++i) {
    while (!queue.Push(i)) {
      // Queue is full.
      Wait(strategy, failed_attempts);
    }

    failed_attempts = 0;
  }

  // Joining guarantees the consumer has processed everything.
  consumer.join();

  const auto end_time = Clock::now();

  // Verify that no items were lost or duplicated.
  assert(consumed_count == ItemCount);

  const uint64 expected_checksum =
      ItemCount * (ItemCount - 1) / 2;

  assert(checksum == expected_checksum);

  const real64 seconds =
      std::chrono::duration<real64>(
          end_time - start_time)
          .count();

  return {
      seconds,
      static_cast<real64>(ItemCount) / seconds,
  };
}

// ------------------------------------------------------------
// Repeated benchmark
// ------------------------------------------------------------

BenchmarkResult RunRepeatedBenchmark(WaitStrategy strategy) {
  std::array<real64, Repetitions> times{};

  std::printf("%s\n", StrategyName(strategy));

  for (usize i = 0; i < Repetitions; ++i) {
    const BenchmarkResult result = RunBenchmark(strategy);

    times[i] = result.seconds;

    std::printf(
        "  run %zu: %.6f s (%.0f items/s)\n",
        i + 1,
        result.seconds,
        result.throughput);
  }

  // Use the median to reduce sensitivity to occasional
  // scheduling interruptions.
  std::sort(times.begin(), times.end());

  const real64 median_seconds = times[Repetitions / 2];

  const real64 median_throughput =
      static_cast<real64>(ItemCount) / median_seconds;

  std::printf(
      "  median: %.6f s (%.0f items/s)\n\n",
      median_seconds,
      median_throughput);

  return {
      median_seconds,
      median_throughput,
  };
}

}  // namespace

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main() {
  std::printf(
      "SPSC Waiting Strategy Benchmark\n"
      "===============================\n\n");

  std::printf(
      "Items: %llu\n",
      static_cast<unsigned long long>(ItemCount));

  std::printf("Queue capacity: %zu\n", QueueCapacity);
  std::printf("Repetitions: %zu\n\n", Repetitions);

  const BenchmarkResult busy =
      RunRepeatedBenchmark(WaitStrategy::BusySpin);

  const BenchmarkResult yielding =
      RunRepeatedBenchmark(WaitStrategy::Yield);

  const BenchmarkResult hybrid =
      RunRepeatedBenchmark(WaitStrategy::Hybrid);

  std::printf(
      "Comparison\n"
      "==========\n\n");

  std::printf(
      "%-12s %12s %18s\n",
      "Strategy",
      "Time (s)",
      "Throughput");

  std::printf(
      "%-12s %12.6f %18.0f\n",
      "Busy Spin",
      busy.seconds,
      busy.throughput);

  std::printf(
      "%-12s %12.6f %18.0f\n",
      "Yield",
      yielding.seconds,
      yielding.throughput);

  std::printf(
      "%-12s %12.6f %18.0f\n",
      "Hybrid",
      hybrid.seconds,
      hybrid.throughput);

  std::printf(
      "\nRelative throughput (vs Yield)\n"
      "Busy Spin: %.3fx\n"
      "Hybrid:    %.3fx\n",
      busy.throughput / yielding.throughput,
      hybrid.throughput / yielding.throughput);

  return 0;
}
