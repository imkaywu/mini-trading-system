#include "mutex_matching_engine.h"

#include <stdexcept>

#include "../matching/limit_order_book.cpp"

namespace engine {

MutexMatchingEngine::MutexMatchingEngine(usize expected_order_count)
    : order_book_(expected_order_count) {}

MutexMatchingEngine::~MutexMatchingEngine() { Stop(); }

void MutexMatchingEngine::Start() {
  if (running_) {
    return;
  }

  running_ = true;
  thread_ = std::thread(&MutexMatchingEngine::Run, this);
}

void MutexMatchingEngine::Stop() {
  if (!running_) {
    return;
  }

  running_ = false;

  // Wake the matching thread if it is currently waiting.
  queue_cv_.notify_one();

  if (thread_.joinable()) {
    thread_.join();
  }
}

bool32 MutexMatchingEngine::SubmitOrder(trading::Order order) {
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);

    order_queue_.push(std::move(order));
  }

  // Notify after releasing the mutex.
  queue_cv_.notify_one();

  return true;
}

usize MutexMatchingEngine::ProcessedCount() const {
  return processed_count_.load(std::memory_order_acquire);
}

void MutexMatchingEngine::Run() {
  while (true) {
    trading::Order order;

    {
      std::unique_lock<std::mutex> lock(queue_mutex_);

      // Sleep until either there is an order to process or the engine is
      // shutting down.
      queue_cv_.wait(lock,
                     [this] { return !order_queue_.empty() || !running_; });

      if (!running_ && order_queue_.empty()) {
        return;
      }

      order = std::move(order_queue_.front());
      order_queue_.pop();
    }

    // The queue lock is NOT held while accessing the book, which is important:
    // other producers should be able to submit orders while the matching engine
    // is processing the current order.
    order_book_.AddOrder(std::move(order));

    processed_count_.fetch_add(1, std::memory_order_release);
  }
}

}  // namespace engine
