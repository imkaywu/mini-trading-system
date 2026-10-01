#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

#include "../matching/limit_order_book.cpp"
#include "../matching/limit_order_book.h"
#include "../types.h"

namespace {
using Clock = std::chrono::steady_clock;
using Nanoseconds = std::chrono::nanoseconds;

struct BenchmarkResult {
  std::vector<int64> latencies_ns;
  std::size_t trade_count = 0;
};

BenchmarkResult BenchmarkRestingOrders(std::size_t order_count) {
  matching::LimitOrderBook book;

  BenchmarkResult result;
  result.latencies_ns.reserve(order_count);

  for (std::size_t i = 0; i < order_count; ++i) {
    trading::Order order{
        .id = 0,
        .side = trading::Side::Buy,
        .price = 100 + static_cast<int64>(i % 100),
        .quantity = 100,
    };

    const auto start = Clock::now();

    const auto trades = book.AddOrder(std::move(order));

    const auto end = Clock::now();

    const auto latency = std::chrono::duration_cast<Nanoseconds>(end - start);

    result.latencies_ns.push_back(latency.count());
    result.trade_count += trades.size();
  }

  return result;
}

BenchmarkResult BenchmarkMatchingOrders(std::size_t order_count) {
  matching::LimitOrderBook book;

  BenchmarkResult result;
  result.latencies_ns.reserve(order_count);

  for (std::size_t i = 0; i < order_count; ++i) {
    book.AddOrder({
        .id = 0,
        .side = trading::Side::Sell,
        .price = 100,
        .quantity = 1,
    });
  }

  for (std::size_t i = 0; i < order_count; ++i) {
    trading::Order order{
        .id = 0,
        .side = trading::Side::Buy,

        .price = 100,
        .quantity = 1,
    };

    const auto start = Clock::now();

    const auto trades = book.AddOrder(std::move(order));

    const auto end = Clock::now();

    const auto latency = std::chrono::duration_cast<Nanoseconds>(end - start);

    result.latencies_ns.push_back(latency.count());
    result.trade_count += trades.size();
  }

  return result;
}

void PrintResult(const char* name, const BenchmarkResult& result) {
  if (result.latencies_ns.empty()) {
    return;
  }

  const auto [min_it, max_it] = std::minmax_element(result.latencies_ns.begin(),
                                                    result.latencies_ns.end());

  int64 total_ns = 0;

  for (const auto latency : result.latencies_ns) {
    total_ns += latency;
  }

  const auto average_ns = static_cast<float64>(total_ns) /
                          static_cast<float64>(result.latencies_ns.size());

  std::cout << "\n";
  std::cout << name << "\n";
  std::cout << "  orders:  " << result.latencies_ns.size() << '\n';
  std::cout << "  trades:  " << result.trade_count << '\n';
  std::cout << "  min:     " << *min_it << " ns\n";
  std::cout << "  average: " << average_ns << " ns\n";
  std::cout << "  max:     " << *max_it << " ns\n";
}

}  // namespace

int main() {
  constexpr std::size_t order_count = 100'000;

  std::cout << "Limit Order Book Benchmark\n";
  std::cout << "==========================\n";

  const auto resting_result = BenchmarkRestingOrders(order_count);

  PrintResult("Resting orders", resting_result);

  const auto matching_result = BenchmarkMatchingOrders(order_count);

  PrintResult("Matching orders", matching_result);

  return 0;
}
