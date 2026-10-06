#pragma once

#include <atomic>
#include <cstddef>
#include <thread>

#include "../concurrency/spsc_queue.h"
#include "../matching/limit_order_book.h"
#include "../types.h"

namespace engine {

class SpscMatchingEngine {
 public:
  explicit SpscMatchingEngine(usize expected_order_count = 0);

  ~SpscMatchingEngine();

  void Start();

  void Stop();

  bool32 SubmitOrder(trading::Order order);

  usize ProcessedCount() const;

 private:
  void Run();

  static constexpr usize OrderQueueCapacity = 1024;

  matching::LimitOrderBook order_book_;

  concurrency::SpscQueue<trading::Order, OrderQueueCapacity> order_queue_;

  std::atomic<bool32> running_{false};

  std::atomic<usize> processed_count_{0};

  std::thread thread_;
};

}  // namespace engine
