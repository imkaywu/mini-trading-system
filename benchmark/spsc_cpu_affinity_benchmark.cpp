#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>

#include <cstring>
#endif

#include "../src/concurrency/spsc_queue.h"
#include "../src/matching/limit_order_book.h"
#include "../src/trading/order.h"
#include "../src/types.h"

// Unity build: include the implementation exactly once.
#include "../src/matching/limit_order_book.cpp"

namespace {

using Clock = std::chrono::steady_clock;
using Timestamp = Clock::time_point;

constexpr uint64 OrderCount = 300'000;
constexpr usize QueueCapacity = 1024;

constexpr int64 OrderPrice = 100;
constexpr int64 OrderQuantity = 1;

enum class AffinityMode {
  Default,
  SameCpu,
  SeparateCpus,
};

struct TimedOrder {
  trading::Order order{};
  Timestamp submitted_at{};
  uint64 sequence = 0;
};

struct BenchmarkResult {
  real64 seconds = 0;
  real64 throughput = 0;

  real64 p50_ns = 0;
  real64 p99_ns = 0;
  real64 p999_ns = 0;
  real64 max_ns = 0;
};

// ------------------------------------------------------------
// CPU affinity
// ------------------------------------------------------------

#ifdef __linux__

// Discover CPUs that this process is permitted to execute on.
//
// Do not assume CPU 0 or CPU 1 is available.
std::vector<int> AvailableCpus() {
  cpu_set_t mask;
  CPU_ZERO(&mask);

  if (sched_getaffinity(0, sizeof(mask), &mask) != 0) {
    std::perror("sched_getaffinity");
    std::abort();
  }

  std::vector<int> cpus;

  for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
    if (CPU_ISSET(cpu, &mask)) {
      cpus.push_back(cpu);
    }
  }

  return cpus;
}

// Called by the thread whose affinity we want to change.
//
// Returns false if the OS rejects the requested affinity.
bool32 PinCurrentThread(int cpu) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(cpu, &mask);

  const int error = pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);

  if (error != 0) {
    std::fprintf(stderr,
                 "pthread_setaffinity_np(CPU %d): %s\n",
                 cpu,
                 std::strerror(error));

    return false;
  }

  return true;
}

#else

std::vector<int> AvailableCpus() { return {}; }

bool32 PinCurrentThread(int cpu) {
  (void)cpu;

  std::fprintf(stderr,
               "Explicit CPU pinning is unsupported on this platform.\n");

  return false;
}

#endif

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
// Statistics
// ------------------------------------------------------------

// Nearest-rank percentile. Samples must already be sorted.
real64 Percentile(const std::vector<real64>& samples, real64 probability) {
  const usize count = samples.size();

  if (count == 0) {
    return 0;
  }

  usize rank =
      static_cast<usize>(std::ceil(probability * static_cast<real64>(count)));

  rank = std::clamp(rank, usize{1}, count);

  return samples[rank - 1];
}

void PrintResult(const char* name, const BenchmarkResult& result) {
  constexpr real64 NsPerUs = 1000.0;

  std::printf("\n%s\n", name);

  std::printf("  Total time: %.6f s\n", result.seconds);

  std::printf("  Throughput: %.0f orders/s\n", result.throughput);

  std::printf("  p50:   %.3f us\n", result.p50_ns / NsPerUs);

  std::printf("  p99:   %.3f us\n", result.p99_ns / NsPerUs);

  std::printf("  p99.9: %.3f us\n", result.p999_ns / NsPerUs);

  std::printf("  Max:   %.3f us\n", result.max_ns / NsPerUs);
}

// ------------------------------------------------------------
// Benchmark
// ------------------------------------------------------------

BenchmarkResult RunBenchmark(AffinityMode mode,
                             int producer_cpu,
                             int consumer_cpu) {
  concurrency::SpscQueue<TimedOrder, QueueCapacity> queue;

  // Consumer exclusively owns the order book and samples.
  matching::LimitOrderBook book(OrderCount);

  std::vector<real64> latencies;
  latencies.reserve(OrderCount);

  // Workers signal readiness after applying affinity.
  std::atomic<int> ready{0};
  std::atomic<bool32> start{false};
  std::atomic<bool32> affinity_ok{true};

  // ----------------------------------------------------------
  // Consumer
  // ----------------------------------------------------------

  std::thread consumer([&] {
    if (mode != AffinityMode::Default) {
      if (!PinCurrentThread(consumer_cpu)) {
        affinity_ok.store(false, std::memory_order_relaxed);
      }
    }

    ready.fetch_add(1, std::memory_order_release);

    while (!start.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }

    TimedOrder item{};

    for (uint64 expected = 0; expected < OrderCount;) {
      if (!queue.Pop(item)) {
        std::this_thread::yield();
        continue;
      }

      if (item.sequence != expected) {
        std::fprintf(stderr,
                     "FIFO error: expected %llu, received %llu\n",
                     static_cast<unsigned long long>(expected),
                     static_cast<unsigned long long>(item.sequence));

        std::abort();
      }

      book.AddOrder(item.order);

      const Timestamp completed_at = Clock::now();

      const real64 latency_ns = std::chrono::duration<real64, std::nano>(
                                    completed_at - item.submitted_at)
                                    .count();

      latencies.push_back(latency_ns);

      ++expected;
    }
  });

  // ----------------------------------------------------------
  // Producer
  // ----------------------------------------------------------

  std::thread producer([&] {
    if (mode != AffinityMode::Default) {
      if (!PinCurrentThread(producer_cpu)) {
        affinity_ok.store(false, std::memory_order_relaxed);
      }
    }

    ready.fetch_add(1, std::memory_order_release);

    while (!start.load(std::memory_order_acquire)) {
      std::this_thread::yield();
    }

    for (uint64 i = 0; i < OrderCount; ++i) {
      TimedOrder item{};

      item.order = MakeOrder();
      item.sequence = i;

      while (true) {
        // Refresh the timestamp before each attempt.
        // Failed submissions are excluded from the
        // per-order latency measurement.
        item.submitted_at = Clock::now();

        if (queue.Push(item)) {
          break;
        }

        std::this_thread::yield();
      }
    }
  });

  // ----------------------------------------------------------
  // Start both workers
  // ----------------------------------------------------------

  while (ready.load(std::memory_order_acquire) != 2) {
    std::this_thread::yield();
  }

  const Timestamp start_time = Clock::now();

  start.store(true, std::memory_order_release);

  producer.join();
  consumer.join();

  const Timestamp end_time = Clock::now();

  if (!affinity_ok.load(std::memory_order_relaxed)) {
    std::fprintf(stderr, "Failed to configure CPU affinity\n");
    std::abort();
  }

  // ----------------------------------------------------------
  // Verify and analyze
  // ----------------------------------------------------------

  if (latencies.size() != OrderCount) {
    std::fprintf(stderr, "Incorrect sample count\n");
    std::abort();
  }

  const real64 seconds =
      std::chrono::duration<real64>(end_time - start_time).count();

  std::sort(latencies.begin(), latencies.end());

  BenchmarkResult result{};

  result.seconds = seconds;

  result.throughput = static_cast<real64>(OrderCount) / seconds;

  result.p50_ns = Percentile(latencies, 0.50);
  result.p99_ns = Percentile(latencies, 0.99);
  result.p999_ns = Percentile(latencies, 0.999);
  result.max_ns = latencies.back();

  return result;
}

}  // namespace

int main() {
  std::printf(
      "SPSC CPU Affinity Benchmark\n"
      "===========================\n\n");

  std::printf("Orders: %llu\n", static_cast<unsigned long long>(OrderCount));

  std::printf("Queue capacity: %zu\n", QueueCapacity);

  // Always run the portable baseline.
  const BenchmarkResult baseline = RunBenchmark(AffinityMode::Default, -1, -1);

  PrintResult("Default scheduling", baseline);

#ifdef __linux__

  const std::vector<int> cpus = AvailableCpus();

  std::printf("\nAvailable logical CPUs:");

  for (int cpu : cpus) {
    std::printf(" %d", cpu);
  }

  std::printf("\n");

  if (cpus.empty()) {
    std::fprintf(stderr, "No available CPUs\n");
    return 1;
  }

  // Both threads pinned to one logical CPU.
  const BenchmarkResult same_cpu =
      RunBenchmark(AffinityMode::SameCpu, cpus[0], cpus[0]);

  PrintResult("Same logical CPU", same_cpu);

  if (cpus.size() >= 2) {
    // Two distinct logical CPUs.
    const BenchmarkResult separate_cpus =
        RunBenchmark(AffinityMode::SeparateCpus, cpus[0], cpus[1]);

    PrintResult("Separate logical CPUs", separate_cpus);

    std::printf(
        "\nRelative throughput (vs default)\n"
        "Same CPU:      %.3fx\n"
        "Separate CPUs: %.3fx\n",
        same_cpu.throughput / baseline.throughput,
        separate_cpus.throughput / baseline.throughput);
  } else {
    std::printf(
        "\nSeparate-CPU experiment skipped: "
        "fewer than two CPUs available.\n");
  }

#else

  std::printf(
      "\nExplicit CPU pinning is unavailable on this platform.\n"
      "Only the default-scheduling benchmark was executed.\n");

#endif

  return 0;
}
