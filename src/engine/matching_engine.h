#pragma once

#include <atomic>
#include <cstddef>
#include <thread>

#include "../matching/limit_order_book.h"
#include "../types.h"

namespace engine {

class MatchingEngine {
 public:
  explicit MatchingEngine(usize expected_order_count = 0);

  ~MatchingEngine();

  // Starts the dedicated matching engine thread.
  void Start();

  // Stops the matching engine thread.
  void Stop();

  // Submits an order to the matching engine.
  // The caller does not access the order book directly.
  void SubmitOrder(trading::Order order);

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

  std::atomic<bool> running_{false};

  std::thread thread_;
};

}  // namespace engine
