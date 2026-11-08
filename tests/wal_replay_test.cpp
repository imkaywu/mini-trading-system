#include "../src/recovery/wal_replay.cpp"

#include <cassert>
#include <filesystem>

#include "../src/matching/limit_order_book.cpp"
#include "../src/recovery/write_ahead_log.cpp"

namespace {

void TestReplay() {
  const std::string path = "wal_replay_test.bin";

  std::filesystem::remove(path);

  // Original execution:
  //
  // 1. Add buy order: ID 1.
  // 2. Cancel order 1.
  //
  // The resulting book is empty, and the next ID is 2.

  {
    recovery::WalWriter writer;

    assert(writer.Open(path));

    assert(writer.AppendAddOrder(trading::Side::Buy, 100, 10));
    assert(writer.AppendCancelOrder(1));

    writer.Close();
  }

  matching::LimitOrderBook recovered_book;
  recovery::WalReplayStats stats{};

  assert(recovery::ReplayWal(path, recovered_book, stats));

  assert(stats.event_count == 2);
  assert(stats.add_order_count == 1);
  assert(stats.cancel_order_count == 1);
  assert(stats.successful_cancel_count == 1);
  assert(stats.generated_trade_count == 0);

  // The next generated order ID must be 2.
  // Add a buy order, then match it with a sell order.
  auto buy_trades = recovered_book.AddOrder({
      .side = trading::Side::Buy,
      .price = 100,
      .quantity = 5,
  });

  assert(buy_trades.empty());

  auto sell_trades = recovered_book.AddOrder({
      .side = trading::Side::Sell,
      .price = 100,
      .quantity = 5,
  });

  assert(sell_trades.size() == 1);

  assert(sell_trades[0].incoming_order_id == 3);
  assert(sell_trades[0].resting_order_id == 2);
  assert(sell_trades[0].price == 100);
  assert(sell_trades[0].quantity == 5);

  std::filesystem::remove(path);
}

void TestReplayWithPartialFill() {
  const std::string path = "wal_partial_replay_test.bin";

  std::filesystem::remove(path);

  {
    recovery::WalWriter writer;

    assert(writer.Open(path));

    // Order 1: Buy 10 at 100.
    assert(writer.AppendAddOrder(trading::Side::Buy, 100, 10));

    // Order 2: Sell 4 at 100.
    // It partially fills order 1, leaving 6.
    assert(writer.AppendAddOrder(trading::Side::Sell, 100, 4));

    writer.Close();
  }

  matching::LimitOrderBook recovered_book;
  recovery::WalReplayStats stats{};

  assert(recovery::ReplayWal(path, recovered_book, stats));

  assert(stats.event_count == 2);
  assert(stats.add_order_count == 2);
  assert(stats.generated_trade_count == 1);

  // Order 1 should still have 6 remaining.
  // Order 3 should match those remaining 6.
  auto trades = recovered_book.AddOrder({
      .side = trading::Side::Sell,
      .price = 100,
      .quantity = 6,
  });

  assert(trades.size() == 1);
  assert(trades[0].incoming_order_id == 3);
  assert(trades[0].resting_order_id == 1);
  assert(trades[0].quantity == 6);

  std::filesystem::remove(path);
}

}  // namespace

int main() {
  TestReplay();
  TestReplayWithPartialFill();

  return 0;
}
