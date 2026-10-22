#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

#include "../src/concurrency/spsc_queue.h"
#include "../src/matching/limit_order_book.h"
#include "../src/trading/order.h"
#include "../src/types.h"

// Unity build: include each implementation exactly once.
#include "../src/matching/limit_order_book.cpp"

namespace {

using Clock = std::chrono::steady_clock;
using Timestamp = Clock::time_point;

constexpr uint64 OrderCount = 300'000;
constexpr usize QueueCapacity = 1024;

constexpr int64 OrderPrice = 100;
constexpr int64 OrderQuantity = 1;

// ------------------------------------------------------------
// Queue payload
// ------------------------------------------------------------
//
// Each order carries its submission timestamp.
//
// sequence is used to verify FIFO delivery independently
// of the order IDs assigned by LimitOrderBook.

struct TimedOrder {
  trading::Order order{};
  Timestamp submitted_at{};
  uint64 sequence = 0;
};

// ------------------------------------------------------------
// Result
// ------------------------------------------------------------

struct LatencyResult {
  real64 mean_ns;
  real64 p50_ns;
  real64 p90_ns;
  real64 p99_ns;
  real64 p999_ns;
  real64 max_ns;

  real64 total_seconds;
  real64 throughput;
};

// ------------------------------------------------------------
// Workload
// ------------------------------------------------------------

trading::Order MakeOrder() {
  trading::Order order{};

  order.side = trading::Side::Buy;
  order.price = OrderPrice;
  order.quantity = OrderQuantity;

  return order;
}

// ------------------------------------------------------------
// Percentile calculation
// ------------------------------------------------------------
//
// Input must already be sorted.
//
// Nearest-rank percentile:
//   rank = ceil(p * N)
//
// For example, p99 uses p = 0.99.

real64 Percentile(const std::vector<real64>& sorted_samples, real64 p) {
  const usize count = sorted_samples.size();

  if (count == 0) {
    return 0;
  }

  usize rank = static_cast<usize>(std::ceil(p * static_cast<real64>(count)));

  if (rank == 0) {
    rank = 1;
  }

  if (rank > count) {
    rank = count;
  }

  return sorted_samples[rank - 1];
}

// ------------------------------------------------------------
// Benchmark
// ------------------------------------------------------------

LatencyResult RunBenchmark(uint64 order_count) {
  concurrency::SpscQueue<TimedOrder, QueueCapacity> queue;

  // The matching thread exclusively owns the order book.
  matching::LimitOrderBook book(order_count);

  // Preallocate before measurement to avoid vector growth
  // during order processing.
  std::vector<real64> latencies;
  latencies.reserve(order_count);

  std::atomic<bool32> start{false};

  // ----------------------------------------------------------
  // Matching thread (consumer)
  // ----------------------------------------------------------

  std::thread consumer([&] {
    while (!start.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }

    TimedOrder item{};

    for (uint64 expected_sequence = 0; expected_sequence < order_count;) {
      if (!queue.Pop(item)) {
        std::this_thread::yield();
        continue;
      }

      // Verify FIFO ordering.
      if (item.sequence != expected_sequence) {
        std::fprintf(stderr,
                     "FIFO error: expected %llu, received %llu\n",
                     static_cast<unsigned long long>(expected_sequence),
                     static_cast<unsigned long long>(item.sequence));

        std::abort();
      }

      // Perform actual matching work.
      book.AddOrder(item.order);

      // Record completion AFTER AddOrder().
      const Timestamp processed_at = Clock::now();

      const real64 latency_ns = std::chrono::duration<real64, std::nano>(
                                    processed_at - item.submitted_at)
                                    .count();

      latencies.push_back(latency_ns);

      ++expected_sequence;
    }
  });

  // ----------------------------------------------------------
  // Producer
  // ----------------------------------------------------------

  const Timestamp start_time = Clock::now();

  start.store(true, std::memory_order_release);

  for (uint64 i = 0; i < order_count; ++i) {
    TimedOrder item{};

    item.order = MakeOrder();
    item.sequence = i;

    while (true) {
      // Refresh the timestamp for each submission attempt.
      //
      // This excludes time spent waiting for a full queue.
      item.submitted_at = Clock::now();

      if (queue.Push(item)) {
        break;
      }

      std::this_thread::yield();
    }
  }

  // The consumer processes exactly order_count orders.
  // join() ensures all latency samples are complete.
  consumer.join();

  const Timestamp end_time = Clock::now();

  // ----------------------------------------------------------
  // Analyze results outside the timed section
  // ----------------------------------------------------------

  if (latencies.size() != order_count) {
    std::fprintf(stderr, "Incorrect latency sample count\n");
    std::abort();
  }

  const real64 total_seconds =
      std::chrono::duration<real64>(end_time - start_time).count();

  real64 sum_ns = 0;

  for (real64 latency : latencies) {
    sum_ns += latency;
  }

  std::sort(latencies.begin(), latencies.end());

  LatencyResult result{};

  result.mean_ns = sum_ns / static_cast<real64>(order_count);

  result.p50_ns = Percentile(latencies, 0.50);
  result.p90_ns = Percentile(latencies, 0.90);
  result.p99_ns = Percentile(latencies, 0.99);
  result.p999_ns = Percentile(latencies, 0.999);
  result.max_ns = latencies.back();

  result.total_seconds = total_seconds;

  result.throughput = static_cast<real64>(order_count) / total_seconds;

  return result;
}

// ------------------------------------------------------------
// Output
// ------------------------------------------------------------

void PrintResult(const LatencyResult& result) {
  std::printf(
      "\nLatency Distribution\n"
      "====================\n\n");

  // Convert nanoseconds to microseconds for readability.
  constexpr real64 NsPerUs = 1000.0;

  std::printf("  Mean:  %12.3f us\n", result.mean_ns / NsPerUs);

  std::printf("  p50:   %12.3f us\n", result.p50_ns / NsPerUs);

  std::printf("  p90:   %12.3f us\n", result.p90_ns / NsPerUs);

  std::printf("  p99:   %12.3f us\n", result.p99_ns / NsPerUs);

  std::printf("  p99.9: %12.3f us\n", result.p999_ns / NsPerUs);

  std::printf("  Max:   %12.3f us\n", result.max_ns / NsPerUs);

  std::printf(
      "\nThroughput\n"
      "==========\n\n");

  std::printf("  Total time: %.6f s\n", result.total_seconds);

  std::printf("  Throughput: %.0f orders/s\n", result.throughput);
}

}  // namespace

// ------------------------------------------------------------
// Main
// ------------------------------------------------------------

int main() {
  std::printf(
      "Matching Engine Latency Benchmark\n"
      "=================================\n\n");

  std::printf("Orders: %llu\n", static_cast<unsigned long long>(OrderCount));

  std::printf("Queue capacity: %zu\n", QueueCapacity);

  std::printf("Workload: resting buy orders, no trades\n");

  const LatencyResult result = RunBenchmark(OrderCount);

  PrintResult(result);

  return 0;
}
