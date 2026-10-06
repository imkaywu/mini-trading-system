#pragma once

#include <atomic>
#include <cstddef>
#include <thread>

#include "../matching/limit_order_book.h"
#include "../types.h"

namespace engine {

class MutexMatchingEngine {
 public:
  explicit MutexMatchingEngine(usize expected_order_count = 0);

  ~MutexMatchingEngine();

  // Starts the dedicated matching engine thread.
  void Start();

  // Stops the matching engine thread.
  void Stop();

  // Submits an order to the matching engine.
  // The caller does not access the order book directly.
  bool32 SubmitOrder(trading::Order order);

  usize ProcessedCount() const;

 private:
  void Run();

  matching::LimitOrderBook order_book_;

  // Orders waiting to be processed by the matching thread.
  std::queue<trading::Order> order_queue_;

  // Protects |order_queue_|.
  std::mutex queue_mutex_;

  // Wakes the matching thread when an order arrives or when the engine is
  // shutting down.
  std::condition_variable queue_cv_;

  std::atomic<bool32> running_{false};

  // Incremented by the matching thread after an order has been processed.
  std::atomic<usize> processed_count_{0};

  std::thread thread_;
};

}  // namespace engine
