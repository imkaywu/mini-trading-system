#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

#include "../src/types.h"
#include "../src/concurrency/spsc_queue.h"
#include "../src/concurrency/mpsc_mutex_queue.h"

namespace {

using Clock = std::chrono::steady_clock;

constexpr usize QueueCapacity = 1024;
constexpr usize MaxProducers = 4;

constexpr uint64 ItemsPerProducer = 250'000;
constexpr usize Repetitions = 5;

struct Item {
  uint64 producer_id = 0;
  uint64 sequence = 0;
};

struct Result {
  real64 seconds = 0;
  real64 throughput = 0;
};

// ------------------------------------------------------------
// Common benchmark
// ------------------------------------------------------------
//
// Queue must provide:
//
//   bool32 Push(Item value);
//   bool32 Pop(Item& value);
//
// The producer count must be 1 for SpscQueue.
//
// The consumer validates FIFO ordering independently for
// each producer.

template <typename Queue>
Result RunBenchmark(usize producer_count) {
  if (producer_count == 0 ||
      producer_count > MaxProducers) {
    std::abort();
  }

  Queue queue;

  const uint64 total_items =
      ItemsPerProducer * producer_count;

  std::atomic<bool32> start{false};
  std::atomic<usize> ready{0};

  // Only the consumer accesses this array.
  std::array<uint64, MaxProducers> expected_sequence{};

  std::thread consumer([&] {
    ready.fetch_add(1, std::memory_order_release);

    while (!start.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }

    Item item{};
    uint64 consumed = 0;

    while (consumed < total_items) {
      if (!queue.Pop(item)) {
        std::this_thread::yield();
        continue;
      }

      if (item.producer_id >= producer_count) {
        std::fprintf(stderr, "Invalid producer ID\n");
        std::abort();
      }

      uint64& expected =
          expected_sequence[item.producer_id];

      if (item.sequence != expected) {
        std::fprintf(
            stderr,
            "FIFO error: producer %llu, "
            "expected %llu, received %llu\n",
            static_cast<unsigned long long>(
                item.producer_id),
            static_cast<unsigned long long>(expected),
            static_cast<unsigned long long>(
                item.sequence));

        std::abort();
      }

      ++expected;
      ++consumed;
    }
  });

  // Construct all producer threads before starting the timer.
  std::vector<std::thread> producers;
  producers.reserve(producer_count);

  for (usize producer_id = 0;
       producer_id < producer_count;
       ++producer_id) {

    producers.emplace_back([&, producer_id] {
      ready.fetch_add(1, std::memory_order_release);

      while (!start.load(std::memory_order_acquire)) {
        std::this_thread::yield();
      }

      for (uint64 sequence = 0;
           sequence < ItemsPerProducer;
           ++sequence) {

        Item item{
            static_cast<uint64>(producer_id),
            sequence,
        };

        // A failed Push() must leave the caller's item
        // available for another attempt.
        while (!queue.Push(item)) {
          std::this_thread::yield();
        }
      }
    });
  }

  // Wait until every thread is ready.
  while (ready.load(std::memory_order_acquire) !=
         producer_count + 1) {
    std::this_thread::yield();
  }

  const auto start_time = Clock::now();

  start.store(true, std::memory_order_release);

  for (std::thread& producer : producers) {
    producer.join();
  }

  consumer.join();

  const auto end_time = Clock::now();

  // Verify that each producer delivered exactly its
  // expected number of items.
  for (usize i = 0; i < producer_count; ++i) {
    if (expected_sequence[i] != ItemsPerProducer) {
      std::fprintf(stderr, "Incorrect item count\n");
      std::abort();
    }
  }

  const real64 seconds =
      std::chrono::duration<real64>(
          end_time - start_time)
          .count();

  return {
      seconds,
      static_cast<real64>(total_items) / seconds,
  };
}

// ------------------------------------------------------------
// Repeated benchmark
// ------------------------------------------------------------

template <typename Queue>
Result RunRepeatedBenchmark(
    const char* name,
    usize producer_count) {

  std::array<real64, Repetitions> times{};

  std::printf(
      "%s (%zu producer%s)\n",
      name,
      producer_count,
      producer_count == 1 ? "" : "s");

  for (usize i = 0; i < Repetitions; ++i) {
    const Result result =
        RunBenchmark<Queue>(producer_count);

    times[i] = result.seconds;

    std::printf(
        "  run %zu: %.6f s, %.0f items/s\n",
        i + 1,
        result.seconds,
        result.throughput);
  }

  std::sort(times.begin(), times.end());

  const real64 median_seconds =
      times[Repetitions / 2];

  const real64 total_items =
      static_cast<real64>(
          ItemsPerProducer * producer_count);

  const Result median{
      median_seconds,
      total_items / median_seconds,
  };

  std::printf(
      "  median: %.6f s, %.0f items/s\n\n",
      median.seconds,
      median.throughput);

  return median;
}

void PrintRow(const char* name, const Result& result) {
  std::printf(
      "%-16s %14.6f %20.0f\n",
      name,
      result.seconds,
      result.throughput);
}

}  // namespace

int main() {
  using Spsc =
      concurrency::SpscQueue<Item, QueueCapacity>;

  using Mpsc =
      concurrency::MpscMutexQueue<Item, QueueCapacity>;

  std::printf(
      "SPSC vs MPSC Benchmark\n"
      "======================\n\n");

  std::printf("Queue capacity: %zu\n", QueueCapacity);

  std::printf(
      "Items per producer: %llu\n",
      static_cast<unsigned long long>(
          ItemsPerProducer));

  std::printf("Repetitions: %zu\n\n", Repetitions);

  // SPSC must only have one producer.
  const Result spsc =
      RunRepeatedBenchmark<Spsc>("SPSC", 1);

  const Result mpsc1 =
      RunRepeatedBenchmark<Mpsc>("MPSC", 1);

  const Result mpsc2 =
      RunRepeatedBenchmark<Mpsc>("MPSC", 2);

  const Result mpsc4 =
      RunRepeatedBenchmark<Mpsc>("MPSC", 4);

  std::printf(
      "Comparison (median)\n"
      "===================\n\n");

  std::printf(
      "%-16s %14s %20s\n",
      "Configuration",
      "Time (s)",
      "Aggregate throughput");

  PrintRow("SPSC (1P)", spsc);
  PrintRow("MPSC (1P)", mpsc1);
  PrintRow("MPSC (2P)", mpsc2);
  PrintRow("MPSC (4P)", mpsc4);

  std::printf(
      "\nSPSC / MPSC (1P): %.3fx\n",
      spsc.throughput / mpsc1.throughput);

  return 0;
}
