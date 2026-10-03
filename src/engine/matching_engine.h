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

 private:
  void Run();

  matching::LimitOrderBook order_book_;

  std::atomic<bool> running_{false};
  std::thread thread_;
};

}  // namespace engine
