#include "spsc_matching_engine.h"

#include <thread>
#include <utility>

namespace engine {

SpscMatchingEngine::SpscMatchingEngine(usize expected_order_count,
                                       TradeCallback trade_callback)
    : order_book_(expected_order_count),
      trade_callback_(std::move(trade_callback)) {}

SpscMatchingEngine::~SpscMatchingEngine() { Stop(); }

void SpscMatchingEngine::Start() {
  if (started_) {
    return;
  }

  started_ = true;
  running_.store(true, std::memory_order_release);

  matching_thread_ = std::thread(&SpscMatchingEngine::Run, this);

  trade_consumer_thread_ =
      std::thread(&SpscMatchingEngine::RunTradeConsumer, this);
}

void SpscMatchingEngine::Stop() {
  if (!started_) {
    return;
  }

  running_.store(false, std::memory_order_release);

  // Matching thread drains accepted orders and publishes their trades.
  if (matching_thread_.joinable()) {
    matching_thread_.join();
  }

  // Trade consumer must remain alive until matching is finished.
  if (trade_consumer_thread_.joinable()) {
    trade_consumer_thread_.join();
  }

  started_ = false;
}

bool32 SpscMatchingEngine::SubmitOrder(trading::Order order) {
  if (!running_.load(std::memory_order_acquire)) {
    return false;
  }

  return order_queue_.Push(std::move(order));
}

usize SpscMatchingEngine::ProcessedCount() const {
  return processed_count_.load(std::memory_order_acquire);
}

usize SpscMatchingEngine::ConsumedTradeCount() const {
  return consumed_trade_count_.load(std::memory_order_acquire);
}

// ------------------------------------------------------------
// Matching thread
// ------------------------------------------------------------

void SpscMatchingEngine::ProcessOrder(trading::Order order) {
  auto trades = order_book_.AddOrder(std::move(order));

  for (const trading::Trade& trade : trades) {
    while (!trade_queue_.Push(trade)) {
      std::this_thread::yield();
    }
  }

  processed_count_.fetch_add(1, std::memory_order_release);
}

void SpscMatchingEngine::Run() {
  trading::Order order;

  while (running_.load(std::memory_order_acquire)) {
    if (order_queue_.Pop(order)) {
      ProcessOrder(order);

      continue;
    }

    // Use yielding rather than busy-spinning continuously.
    std::this_thread::yield();
  }

  // Drain orders that were queued before shutdown.
  while (order_queue_.Pop(order)) {
    ProcessOrder(std::move(order));
  }

  matching_done_.store(true, std::memory_order_release);
}

// ------------------------------------------------------------
// Trade consumer thread
// ------------------------------------------------------------

void SpscMatchingEngine::RunTradeConsumer() {
  trading::Trade trade{};

  while (true) {
    if (trade_queue_.Pop(trade)) {
      // Application-defined trade processing.
      // Callback must NOT throw or call Stop().
      if (trade_callback_) {
        trade_callback_(trade);
      }

      consumed_trade_count_.fetch_add(1, std::memory_order_release);

      continue;
    }

    if (matching_done_.load(std::memory_order_acquire)) {
      break;
    }

    std::this_thread::yield();
  }
}

}  // namespace engine
