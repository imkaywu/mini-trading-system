#include "spsc_matching_engine.h"

#include <thread>
#include <utility>

namespace engine {

SpscMatchingEngine::SpscMatchingEngine(usize expected_order_count)
    : order_book_(expected_order_count) {}

SpscMatchingEngine::~SpscMatchingEngine() { Stop(); }

void SpscMatchingEngine::Start() {
  if (running_) {
    return;
  }

  running_ = true;

  thread_ = std::thread(&SpscMatchingEngine::Run, this);
}

void SpscMatchingEngine::Stop() {
  if (!running_) {
    return;
  }

  running_ = false;

  if (thread_.joinable()) {
    thread_.join();
  }
}

bool32 SpscMatchingEngine::SubmitOrder(trading::Order order) {
  return order_queue_.push(std::move(order));
}

usize SpscMatchingEngine::ProcessedCount() const {
  return processed_count_.load(std::memory_order_acquire);
}

void SpscMatchingEngine::Run() {
  while (running_) {
    trading::Order order;

    if (order_queue_.pop(order)) {
      order_book_.AddOrder(std::move(order));

      processed_count_.fetch_add(1, std::memory_order_release);

      continue;
    }

    // Use yielding rather than busy-spinning continuously.
    std::this_thread::yield();
  }

  // Drain orders that were queued before shutdown.
  trading::Order order;
  while (order_queue_.pop(order)) {
    order_book_.AddOrder(std::move(order));

    processed_count_.fetch_add(1, std::memory_order_release);
  }
}

}  // namespace engine
