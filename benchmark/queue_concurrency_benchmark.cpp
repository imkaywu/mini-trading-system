#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>

#include "../src/concurrency/mutex_queue.h"
#include "../src/concurrency/spsc_queue.h"
#include "../src/types.h"

namespace {

using Clock = std::chrono::steady_clock;

struct BenchmarkResult {
  real64 seconds;
  real64 throughput;
};

// ============================================================
// Mutex + condition variable benchmark
// ============================================================

BenchmarkResult RunMutexBenchmark(uint64 item_count) {
  concurrency::MutexQueue<uint64> queue;

  std::atomic<uint64> consumed_count{0};

  const auto start_time = Clock::now();

  std::thread consumer([&] {
    uint64 value;

    while (queue.Pop(value)) {
      //(void)value;
      consumed_count.fetch_add(1, std::memory_order_relaxed);
    }
  });

  for (uint64 i = 0; i < item_count; ++i) {
    queue.Push(i);
  }

  while (consumed_count.load(std::memory_order_acquire) != item_count) {
    std::this_thread::yield();
  }

  const auto end_time = Clock::now();

  queue.Stop();
  consumer.join();

  const real64 seconds =
      std::chrono::duration<real64>(end_time - start_time).count();

  const real64 throughput = static_cast<real64>(item_count) / seconds;

  return {seconds, throughput};
}

// ============================================================
// SPSC benchmark
// ============================================================
//
// Exactly one producer thread calls Push().
// Exactly one consumer thread calls Pop().
//
// If the queue is full, the producer waits by yielding.
// This is intentional: this is measuring the effect of
// queue capacity and backpressure.

template <usize Capacity>
BenchmarkResult RunSpscBenchmark(uint64 item_count) {
  concurrency::SpscQueue<uint64, Capacity> queue;

  std::atomic<uint64> consumed_count{0};
  std::atomic<bool32> producer_done{false};

  const auto start_time = Clock::now();

  std::thread consumer([&] {
    uint64 value;

    while (true) {
      if (queue.Pop(value)) {
        (void)value;

        consumed_count.fetch_add(1, std::memory_order_relaxed);
        continue;
      }

      // Pop() failed, so the queue was observed empty.
      //
      // We can exit only when the producer has finished.
      // Otherwise, more items may arrive.
      if (producer_done.load(std::memory_order_acquire)) {
        break;
      }

      std::this_thread::yield();
    }
  });

  for (uint64 i = 0; i < item_count; ++i) {
    while (!queue.Push(i)) {
      std::this_thread::yield();
    }
  }

  producer_done.store(true, std::memory_order_release);

  while (consumed_count.load(std::memory_order_acquire) != item_count) {
    std::this_thread::yield();
  }

  const auto end_time = Clock::now();

  consumer.join();

  const real64 seconds =
      std::chrono::duration<real64>(end_time - start_time).count();

  const real64 throughput = static_cast<real64>(item_count) / seconds;

  return {seconds, throughput};
}

// ============================================================
// Output
// ============================================================

void PrintResult(const char* name, BenchmarkResult result) {
  std::printf("%-18s %8.3f s  %12.0f orders/s\n",
              name,
              result.seconds,
              result.throughput);
}

}  // namespace

int main() {
  constexpr uint64 item_count = 1'000'000;

  std::printf(
      "Queue Concurrency Benchmark\n"
      "===========================\n\n");

  std::printf("items: %llu\n\n", static_cast<unsigned long long>(item_count));

  // --------------------------------------------------------
  // Mutex + condition variable
  // --------------------------------------------------------

  std::printf("--- Mutex + Condition Variable ---\n\n");

  const BenchmarkResult mutex_result = RunMutexBenchmark(item_count);

  PrintResult("Mutex/CV", mutex_result);

  std::printf("\n");

  // --------------------------------------------------------
  // SPSC
  // --------------------------------------------------------

  std::printf("--- SPSC Queue ---\n\n");

  const BenchmarkResult spsc_64 = RunSpscBenchmark<64>(item_count);

  const BenchmarkResult spsc_256 = RunSpscBenchmark<256>(item_count);

  const BenchmarkResult spsc_1024 = RunSpscBenchmark<1024>(item_count);

  const BenchmarkResult spsc_4096 = RunSpscBenchmark<4096>(item_count);

  const BenchmarkResult spsc_16384 = RunSpscBenchmark<16384>(item_count);

  PrintResult("SPSC 64", spsc_64);
  PrintResult("SPSC 256", spsc_256);
  PrintResult("SPSC 1024", spsc_1024);
  PrintResult("SPSC 4096", spsc_4096);
  PrintResult("SPSC 16384", spsc_16384);

  std::printf("\n");

  // --------------------------------------------------------
  // Comparison
  // --------------------------------------------------------

  std::printf(
      "Comparison\n"
      "==========\n");

  std::printf("SPSC 64 / Mutex:     %.3fx\n",
              spsc_64.throughput / mutex_result.throughput);

  std::printf("SPSC 256 / Mutex:    %.3fx\n",
              spsc_256.throughput / mutex_result.throughput);

  std::printf("SPSC 1024 / Mutex:   %.3fx\n",
              spsc_1024.throughput / mutex_result.throughput);

  std::printf("SPSC 4096 / Mutex:   %.3fx\n",
              spsc_4096.throughput / mutex_result.throughput);

  std::printf("SPSC 16384 / Mutex:  %.3fx\n",
              spsc_16384.throughput / mutex_result.throughput);

  return 0;
}
