#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

#include "../matching/limit_order_book.cpp"
#include "../matching/limit_order_book.h"
#include "../types.h"

namespace benchmark {

struct AllocationStats {
  usize allocations = 0;
  usize allocated_bytes = 0;
  usize deallocations = 0;
  usize deallocated_bytes = 0;
};

AllocationStats allocation_stats;

void reset_allocation_stats() { allocation_stats = {}; }

}  // namespace benchmark

// -----------------------------------------------------------------------------
// Allocation instrumentation.
//
// These operators are used by the benchmark executable only.
// They let us observe dynamic allocations made by the order book.
//
// This is instrumentation, not an allocator we would use in production.
// -----------------------------------------------------------------------------

void* operator new(usize size) {
  void* ptr = std::malloc(size);

  if (ptr == nullptr) {
    throw std::bad_alloc();
  }

  ++benchmark::allocation_stats.allocations;
  benchmark::allocation_stats.allocated_bytes += size;

  return ptr;
}

void* operator new[](usize size) {
  void* ptr = std::malloc(size);

  if (ptr == nullptr) {
    throw std::bad_alloc();
  }

  ++benchmark::allocation_stats.allocations;
  benchmark::allocation_stats.allocated_bytes += size;

  return ptr;
}

void operator delete(void* ptr) noexcept { std::free(ptr); }

void operator delete[](void* ptr) noexcept { std::free(ptr); }

void operator delete(void* ptr, usize size) noexcept {
  if (ptr != nullptr) {
    ++benchmark::allocation_stats.deallocations;
    benchmark::allocation_stats.deallocated_bytes += size;
  }

  std::free(ptr);
}

void operator delete[](void* ptr, usize size) noexcept {
  if (ptr != nullptr) {
    ++benchmark::allocation_stats.deallocations;
    benchmark::allocation_stats.deallocated_bytes += size;
  }

  std::free(ptr);
}

namespace {
using Clock = std::chrono::steady_clock;
using Nanoseconds = std::chrono::nanoseconds;

struct BenchmarkResult {
  std::vector<int64> latencies_ns;

  usize trade_count = 0;

  benchmark::AllocationStats allocation_stats;
};

BenchmarkResult BenchmarkRestingOrders(usize order_count,
                                       bool32 reserve_order_index) {
  matching::LimitOrderBook book(reserve_order_index ? order_count : 0);

  BenchmarkResult result;
  result.latencies_ns.reserve(order_count);

  // Do not count allocation caused by the benchmark's latency vector.
  benchmark::reset_allocation_stats();

  for (usize i = 0; i < order_count; ++i) {
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

  result.allocation_stats = benchmark::allocation_stats;

  return result;
}

BenchmarkResult BenchmarkMatchingOrders(usize order_count,
                                        bool32 reserve_order_index) {
  matching::LimitOrderBook book(reserve_order_index ? order_count : 0);

  // Build the order book before starting the measurement.
  for (usize i = 0; i < order_count; ++i) {
    book.AddOrder({
        .id = 0,
        .side = trading::Side::Sell,
        .price = 100,
        .quantity = 1,
    });
  }

  BenchmarkResult result;
  result.latencies_ns.reserve(order_count);

  benchmark::reset_allocation_stats();

  for (usize i = 0; i < order_count; ++i) {
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

  result.allocation_stats = benchmark::allocation_stats;

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

  const auto avg_ns = static_cast<real64>(total_ns) /
                      static_cast<real64>(result.latencies_ns.size());

  std::cout << "\n";
  std::cout << name << "\n";
  std::cout << "  orders:  " << result.latencies_ns.size() << '\n';
  std::cout << "  trades:  " << result.trade_count << '\n';
  std::cout << "  min latency:     " << *min_it << " ns\n";
  std::cout << "  avg latency: " << avg_ns << " ns\n";
  std::cout << "  max latency:     " << *max_it << " ns\n";
  std::cout << "  allocations:      " << result.allocation_stats.allocations
            << '\n';
  std::cout << "  allocated bytes:  " << result.allocation_stats.allocated_bytes
            << '\n';
  std::cout << "  deallocations:    " << result.allocation_stats.deallocations
            << '\n';
  std::cout << "  deallocated bytes:"
            << result.allocation_stats.deallocated_bytes << '\n';
}

void PrintComparison(const BenchmarkResult& unreserved,
                     const BenchmarkResult& reserved) {
  std::cout << "\nAllocation comparison\n";
  std::cout << "======================\n";

  std::cout << "Unreserved allocation: "
            << unreserved.allocation_stats.allocations << "\n";

  std::cout << "Reserved allocation: " << reserved.allocation_stats.allocations
            << "\n";

  std::cout << "Unreserved allocation: "
            << unreserved.allocation_stats.allocated_bytes << "\n";

  std::cout << "Reserved allocation: "
            << reserved.allocation_stats.allocated_bytes << "\n";
}

}  // namespace

int main() {
  constexpr std::size_t order_count = 100'000;

  std::cout << "Limit Order Book Memory Benchmark\n";
  std::cout << "=================================\n";

  const auto unreserved_resting = BenchmarkRestingOrders(order_count, false);

  const auto reserved_resting = BenchmarkRestingOrders(order_count, true);

  std::cout << "\n--- Resting Orders ---\n";

  PrintResult("Unreserved", unreserved_resting);

  PrintResult("Reserved", reserved_resting);

  PrintComparison(unreserved_resting, reserved_resting);

  const auto unreserved_matching = BenchmarkRestingOrders(order_count, false);

  const auto reserved_matching = BenchmarkRestingOrders(order_count, true);

  std::cout << "\n--- Matching Orders ---\n";

  PrintResult("Unreserved", unreserved_matching);

  PrintResult("Reserved", reserved_matching);

  PrintComparison(unreserved_matching, reserved_matching);

  return 0;
}
