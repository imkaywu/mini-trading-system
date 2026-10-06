#include <chrono>
#include <cstddef>
#include <iomanip>
#include <memory>
#include <thread>
#include <vector>

#include "../src/engine/mutex_matching_engine.cpp"
#include "../src/engine/spsc_matching_engine.cpp"

namespace benchmark {

using Clock = std::chrono::steady_clock;

struct BenchmarkResult {
  usize orders = 0;

  real64 submit_seconds = 0.0;
  real64 total_seconds = 0.0;

  real64 submit_orders_per_second = 0.0;
  real64 processed_orders_per_second = 0.0;
};

template <typename Engine>
BenchmarkResult RunBenchmark(usize order_count, usize expected_order_count) {
  Engine engine(expected_order_count);

  engine.Start();

  const auto start_time = Clock::now();

  usize submitted = 0;

  while (submitted < order_count) {
    trading::Order order{
        .id = 0,
        .side = trading::Side::Buy,
        .price = 100 + static_cast<int64>(submitted % 10),
        .quantity = 1,
    };

    if (engine.SubmitOrder(std::move(order))) {
      ++submitted;
    } else {
      // The SPSC queue is bounded.
      //
      // If it is full, give the matching thread a chance to consume some
      // orders.
      std::this_thread::yield();
    }
  }

  const auto submit_end_time = Clock::now();

  // Wait until the matching thread has processed the entire submitted
  // batch.
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
      .orders = order_count,
      .submit_seconds = submit_seconds,
      .total_seconds = total_seconds,
      .submit_orders_per_second =
          static_cast<real64>(order_count) / submit_seconds,
      .processed_orders_per_second =
          static_cast<real64>(order_count) / total_seconds,
  };
}

void PrintResult(const char* name, const BenchmarkResult& result) {
  std::cout << name << '\n';
  std::cout << "  orders: " << result.orders << '\n';

  std::cout << std::fixed << std::setprecision(3);

  std::cout << "  submit time: " << result.submit_seconds << " s\n";

  std::cout << "  total time:  " << result.total_seconds << " s\n";

  std::cout << "  submit throughput: " << result.submit_orders_per_second
            << " orders/s\n";

  std::cout << "  end-to-end throughput: " << result.processed_orders_per_second
            << " orders/s\n";

  std::cout << '\n';
}

}  // namespace benchmark

int main() {
  constexpr usize OrderCount = 300'000;

  std::cout << "Matching Engine Concurrency Benchmark\n"
            << "======================================\n\n";

  std::cout << "--- Mutex + Condition Variable ---\n\n";

  const auto mutex_result =
      benchmark::RunBenchmark<engine::MutexMatchingEngine>(OrderCount,
                                                           OrderCount);

  benchmark::PrintResult("Mutex/CV", mutex_result);

  std::cout << "--- SPSC Queue ---\n\n";

  const auto spsc_result = benchmark::RunBenchmark<engine::SpscMatchingEngine>(
      OrderCount, OrderCount);

  benchmark::PrintResult("SPSC", spsc_result);

  std::cout << "Comparison\n";
  std::cout << "==========\n";

  const double throughput_ratio = spsc_result.processed_orders_per_second /
                                  mutex_result.processed_orders_per_second;

  std::cout << std::fixed << std::setprecision(3);

  std::cout << "SPSC / Mutex throughput: " << throughput_ratio << "x\n";

  return 0;
}
