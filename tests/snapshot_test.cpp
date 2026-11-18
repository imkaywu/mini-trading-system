#include "../src/recovery/snapshot.h"

#include <cassert>
#include <filesystem>

#include "../src/matching/limit_order_book.cpp"
#include "../src/recovery/snapshot.cpp"

namespace {

void TestSnapshotRestore() {
  const std::string path = "snapshot_test.bin";

  std::filesystem::remove(path);

  matching::LimitOrderBook original_book;

  // Order 1: Buy 10 at 100.
  original_book.AddOrder({
      .side = trading::Side::Buy,
      .price = 100,
      .quantity = 10,
  });

  // Order 2: Buy 5 at 100.
  original_book.AddOrder({
      .side = trading::Side::Buy,
      .price = 100,
      .quantity = 5,
  });

  // Order 3: Sell 12 at 100.
  // Fully fills order 1 and partially fills order 2.
  auto trades = original_book.AddOrder({
      .side = trading::Side::Sell,
      .price = 100,
      .quantity = 12,
  });

  assert(trades.size() == 2);
  assert(trades[0].resting_order_id == 1);
  assert(trades[0].quantity == 10);
  assert(trades[1].resting_order_id == 2);
  assert(trades[1].quantity == 2);

  // Order 2 should have 3 remaining.
  assert(recovery::WriteSnapshot(path, original_book, 0));

  matching::LimitOrderBook restored_book;

  uint64 last_sequence_number = 0;
  assert(recovery::ReadSnapshot(path, restored_book, last_sequence_number));
  assert(last_sequence_number == 0);

  // The next order should receive ID 4.
  // It should match the remaining quantity of order 2.
  auto original_trades = original_book.AddOrder({
      .side = trading::Side::Sell,
      .price = 100,
      .quantity = 3,
  });

  auto restored_trades = restored_book.AddOrder({
      .side = trading::Side::Sell,
      .price = 100,
      .quantity = 3,
  });

  assert(original_trades.size() == 1);
  assert(restored_trades.size() == 1);

  assert(original_trades[0].incoming_order_id == 4);
  assert(restored_trades[0].incoming_order_id == 4);

  assert(original_trades[0].resting_order_id == 2);
  assert(restored_trades[0].resting_order_id == 2);

  assert(original_trades[0].price == restored_trades[0].price);
  assert(original_trades[0].quantity == restored_trades[0].quantity);

  std::filesystem::remove(path);
}

void TestEmptyBookSnapshot() {
  const std::string path = "empty_snapshot_test.bin";

  std::filesystem::remove(path);

  matching::LimitOrderBook original_book;

  // Consume an ID with an order that is immediately fully filled.
  original_book.AddOrder({
      .side = trading::Side::Buy,
      .price = 100,
      .quantity = 5,
  });

  original_book.AddOrder({
      .side = trading::Side::Sell,
      .price = 100,
      .quantity = 5,
  });

  assert(recovery::WriteSnapshot(path, original_book, 0));

  matching::LimitOrderBook restored_book;

  uint64 last_sequence_number = 0;
  assert(recovery::ReadSnapshot(path, restored_book, last_sequence_number));
  assert(last_sequence_number == 0);

  // The book is empty, but the next ID must still be 3.
  auto trades = restored_book.AddOrder({
      .side = trading::Side::Buy,
      .price = 100,
      .quantity = 5,
  });

  assert(trades.empty());

  auto next_trades = restored_book.AddOrder({
      .side = trading::Side::Sell,
      .price = 100,
      .quantity = 5,
  });

  assert(next_trades.size() == 1);
  assert(next_trades[0].resting_order_id == 3);
  assert(next_trades[0].incoming_order_id == 4);

  std::filesystem::remove(path);
}

}  // namespace

int main() {
  TestSnapshotRestore();
  TestEmptyBookSnapshot();

  return 0;
}
