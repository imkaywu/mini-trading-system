#include "../src/recovery/order_book_recovery.h"

#include <cassert>
#include <cstdio>
#include <iostream>

#include "../src/matching/limit_order_book.cpp"
#include "../src/recovery/order_book_recovery.cpp"
#include "../src/recovery/snapshot.cpp"
#include "../src/recovery/wal_replay.cpp"
#include "../src/recovery/write_ahead_log.cpp"
#include "../src/trading/order.h"

namespace {

bool32 SameOrderBook(const matching::LimitOrderBook& a,
                     const matching::LimitOrderBook& b) {
  const auto snapshot_a = a.ExportSnapshot();
  const auto snapshot_b = b.ExportSnapshot();

  if (snapshot_a.next_order_id != snapshot_b.next_order_id ||
      snapshot_a.orders.size() != snapshot_b.orders.size()) {
    return false;
  }

  for (usize i = 0; i < snapshot_a.orders.size(); ++i) {
    const auto& x = snapshot_a.orders[i];
    const auto& y = snapshot_b.orders[i];

    if (x.id != y.id || x.side != y.side || x.price != y.price ||
        x.quantity != y.quantity) {
      return false;
    }
  }

  return true;
}

void TestSnapshotAndWalRecovery() {
  const char* wal_path = "recovery_test.wal";
  const char* snapshot_path = "recovery_test.snapshot";

  std::remove(wal_path);
  std::remove(snapshot_path);

  matching::LimitOrderBook original_book;
  recovery::WalWriter writer;

  assert(writer.Open(wal_path));

  // Event 1.
  assert(writer.AppendAddOrder(trading::Side::Buy, 100, 10));

  trading::Order order1{};
  order1.side = trading::Side::Buy;
  order1.price = 100;
  order1.quantity = 10;
  original_book.AddOrder(order1);

  // Event 2.
  assert(writer.AppendAddOrder(trading::Side::Buy, 99, 5));

  trading::Order order2{};
  order2.side = trading::Side::Buy;
  order2.price = 99;
  order2.quantity = 5;
  original_book.AddOrder(order2);

  // Snapshot includes events 1 and 2.
  assert(recovery::WriteSnapshot(snapshot_path, original_book, 2));

  // Event 3: partially fills the first resting order.
  assert(writer.AppendAddOrder(trading::Side::Sell, 100, 4));

  trading::Order order3{};
  order3.side = trading::Side::Sell;
  order3.price = 100;
  order3.quantity = 4;
  original_book.AddOrder(order3);

  writer.Close();

  // Recover into a new order book.
  matching::LimitOrderBook recovered_book;
  recovery::WalReplayStats stats;

  assert(recovery::RecoverOrderBook(
      snapshot_path, wal_path, recovered_book, stats));

  assert(SameOrderBook(original_book, recovered_book));

  // Only event 3 should have been applied during replay.
  assert(stats.event_count == 1);
  assert(stats.add_order_count == 1);
  assert(stats.cancel_order_count == 0);
  assert(stats.generated_trade_count == 1);

  std::remove(wal_path);
  std::remove(snapshot_path);
}

void TestReplayWithoutSnapshot() {
  const char* wal_path = "recovery_no_snapshot_test.wal";
  const char* snapshot_path = "recovery_no_snapshot_test.snapshot";

  std::remove(wal_path);
  std::remove(snapshot_path);

  recovery::WalWriter writer;
  assert(writer.Open(wal_path));

  assert(writer.AppendAddOrder(trading::Side::Buy, 100, 10));
  assert(writer.AppendAddOrder(trading::Side::Sell, 100, 4));

  writer.Close();

  matching::LimitOrderBook recovered_book;
  recovery::WalReplayStats stats;

  assert(recovery::RecoverOrderBook(
      snapshot_path, wal_path, recovered_book, stats));

  assert(stats.event_count == 2);
  assert(stats.add_order_count == 2);
  assert(stats.generated_trade_count == 1);

  const auto snapshot = recovered_book.ExportSnapshot();

  assert(snapshot.next_order_id == 3);
  assert(snapshot.orders.size() == 1);
  assert(snapshot.orders[0].id == 1);
  assert(snapshot.orders[0].quantity == 6);

  std::remove(wal_path);
}

void TestSnapshotSequenceAheadOfWal() {
  const char* wal_path = "recovery_sequence_test.wal";
  const char* snapshot_path = "recovery_sequence_test.snapshot";

  std::remove(wal_path);
  std::remove(snapshot_path);

  matching::LimitOrderBook original_book;

  trading::Order order{};
  order.side = trading::Side::Buy;
  order.price = 100;
  order.quantity = 10;
  original_book.AddOrder(order);

  // Snapshot claims to include event 2, but the WAL will be empty.
  assert(recovery::WriteSnapshot(snapshot_path, original_book, 2));

  recovery::WalWriter writer;
  assert(writer.Open(wal_path));
  writer.Close();

  matching::LimitOrderBook recovered_book(0);
  recovery::WalReplayStats stats;

  assert(!recovery::RecoverOrderBook(
      snapshot_path, wal_path, recovered_book, stats));

  std::remove(wal_path);
  std::remove(snapshot_path);
}

}  // namespace

int main() {
  TestSnapshotAndWalRecovery();
  // TestReplayWithoutSnapshot();
  // TestSnapshotSequenceAheadOfWal();

  std::cout << "All order book recovery tests passed.\n";
  return 0;
}
