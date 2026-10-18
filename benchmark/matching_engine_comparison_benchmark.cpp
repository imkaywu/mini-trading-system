#include <chrono>
#include <cstdio>
#include <thread>
#include <utility>

#include "../src/engine/mutex_matching_engine.cpp"
#include "../src/engine/mutex_matching_engine.h"
#include "../src/engine/spsc_matching_engine.cpp"
#include "../src/engine/spsc_matching_engine.h"
#include "../src/matching/limit_order_book.cpp"
#include "../src/matching/limit_order_book.h"
#include "../src/trading/order.h"
#include "../src/types.h"

namespace {

using Clock = std::chrono::steady_clock;

constexpr uint64 OrderCount = 300'000;
constexpr int64 OrderPrice = 100;
constexpr int64 OrderQuantity = 1;

struct BenchmarkResult {
  const char* name;

  real64 submit_seconds;
  real64 total_seconds;

  real64 submit_throughput;
  real64 total_throughput;

  usize pending_after_submit;
};

trading::Order MakeOrder() {
  trading::Order order{};

  order.side = trading::Side::Buy;
  order.price = OrderPrice;
  order.quantity = OrderQuantity;

  return order;
}

// ------------------------------------------------------------
// Direct single-threaded baseline
// ------------------------------------------------------------
//
// No queue, mutex, condition variable, or worker thread.
//
// This measures the matching workload without concurrency
// infrastructure.

BenchmarkResult RunDirectBenchmark(uint64 order_count) {
  matching::LimitOrderBook book(order_count);

  const auto start_time = Clock::now();

  for (uint64 i = 0; i < order_count; ++i) {
    book.AddOrder(MakeOrder());
  }

  const auto end_time = Clock::now();

  const real64 seconds =
      std::chrono::duration<real64>(end_time - start_time).count();

  const real64 throughput = static_cast<real64>(order_count) / seconds;

  return {
      "Direct",
      seconds,
      seconds,
      throughput,
      throughput,
      0,
  };
}

template <typename Engine>
BenchmarkResult RunEngineBenchmark(const char* name, uint64 order_count) {
  Engine engine(order_count);

  engine.Start();

  const auto start_time = Clock::now();

  for (uint64 i = 0; i < order_count; ++i) {
    trading::Order order = MakeOrder();

    while (!engine.SubmitOrder(order)) {
      std::this_thread::yield();
    }
  }

  const auto submit_end_time = Clock::now();

  const usize processed_after_submit = engine.ProcessedCount();

  const usize pending_after_submit =
      static_cast<usize>(order_count) - processed_after_submit;

  while (engine.ProcessedCount() < order_count) {
    std::this_thread::yield();
  }

  const auto end_time = Clock::now();

  engine.Stop();

  const real64 submit_seconds =
      std::chrono::duration<real64>(submit_end_time - start_time).count();

  const real64 total_seconds =
      std::chrono::duration<real64>(end_time - start_time).count();

  return {
      name,
      submit_seconds,
      total_seconds,
      static_cast<real64>(order_count) / submit_seconds,
      static_cast<real64>(order_count) / total_seconds,
      pending_after_submit,
  };
}

void PrintResult(const BenchmarkResult& result) {
  std::printf("%s\n", result.name);

  std::printf("  submit time:           %.6f s\n", result.submit_seconds);

  std::printf("  total time:            %.6f s\n", result.total_seconds);

  std::printf("  submit throughput:     %.0f orders/s\n",
              result.submit_throughput);

  std::printf("  end-to-end throughput: %.0f orders/s\n",
              result.total_throughput);

  std::printf("  pending after submit:  %zu\n\n", result.pending_after_submit);
}
void PrintComparison(const BenchmarkResult& direct,
                     const BenchmarkResult& mutex,
                     const BenchmarkResult& spsc) {
  std::printf(
      "Comparison\n"
      "==========\n\n");

  std::printf("%-12s %18s %18s\n", "Engine", "Throughput", "vs Direct");

  std::printf(
      "%-12s %18.0f %17.3fx\n", direct.name, direct.total_throughput, 1.0);

  std::printf("%-12s %18.0f %17.3fx\n",
              mutex.name,
              mutex.total_throughput,
              mutex.total_throughput / direct.total_throughput);

  std::printf("%-12s %18.0f %17.3fx\n",
              spsc.name,
              spsc.total_throughput,
              spsc.total_throughput / direct.total_throughput);

  std::printf("\nSPSC / Mutex: %.3fx\n",
              spsc.total_throughput / mutex.total_throughput);
}

}  // namespace

int main() {
  std::printf(
      "Matching Engine Comparison Benchmark\n"
      "====================================\n\n");

  std::printf("Orders: %llu\n", static_cast<unsigned long long>(OrderCount));

  std::printf("Workload: resting buy orders, no trades\n\n");

  // 1. Single-threaded baseline.
  const BenchmarkResult direct = RunDirectBenchmark(OrderCount);

  PrintResult(direct);

  // 2. Mutex + condition variable.
  const BenchmarkResult mutex =
      RunEngineBenchmark<engine::MutexMatchingEngine>("Mutex/CV", OrderCount);

  PrintResult(mutex);

  // 3. SPSC queue.
  const BenchmarkResult spsc =
      RunEngineBenchmark<engine::SpscMatchingEngine>("SPSC", OrderCount);

  PrintResult(spsc);

  PrintComparison(direct, mutex, spsc);

  return 0;
}
