#pragma once

#include <atomic>
#include <cstddef>
#include <thread>

#include "../concurrency/spsc_queue.h"
#include "../matching/limit_order_book.h"
#include "../trading/trade.h"
#include "../types.h"

namespace engine {

class SpscMatchingEngine {
 public:
  using TradeCallback = std::function<void(const trading::Trade&)>;

  explicit SpscMatchingEngine(usize expected_order_count = 0,
                              TradeCallback trade_callback = {});

  ~SpscMatchingEngine();

  void Start();

  void Stop();

  bool32 SubmitOrder(trading::Order order);

  usize ProcessedCount() const;
  usize ConsumedTradeCount() const;

 private:
  void Run();
  void RunTradeConsumer();

  void ProcessOrder(trading::Order order);

  static constexpr usize OrderQueueCapacity = 1024;
  static constexpr usize TradeQueueCapacity = 1024;

  matching::LimitOrderBook order_book_;

  concurrency::SpscQueue<trading::Order, OrderQueueCapacity> order_queue_;
  concurrency::SpscQueue<trading::Trade, TradeQueueCapacity> trade_queue_;

  TradeCallback trade_callback_;

  // Tracks whether engine has been started, preventing accidental
  // restarts.
  bool32 started_ = false;

  // Tells the matching thread whether it should continue its normal
  // processing loop.
  std::atomic<bool32> running_{false};

  // Set by matching thread after all accepted orders and their
  // generated trades have been processed/published.
  std::atomic<bool32> matching_done_{false};

  std::atomic<usize> processed_count_{0};
  std::atomic<usize> consumed_trade_count_{0};

  std::thread matching_thread_;
  std::thread trade_consumer_thread_;
};

}  // namespace engine
