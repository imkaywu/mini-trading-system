#include "wal_replay.h"

#include <utility>

#include "write_ahead_log.h"

namespace recovery {

bool32 ReplayWal(const std::string& path,
                 matching::LimitOrderBook& order_book,
                 WalReplayStats& stats,
                 uint64 after_sequence_number) {
  stats = {};

  WalReader reader;

  if (!reader.Open(path)) {
    return false;
  }

  uint64 last_sequence_number = 0;
  WalEvent event{};

  while (true) {
    const WalReadResult result = reader.ReadNext(event);

    if (result == WalReadResult::End) {
      break;
    }

    if (result == WalReadResult::Error) {
      reader.Close();
      return false;
    }

    last_sequence_number = event.sequence_number;

    // This event is already included in the snapshot.
    if (event.sequence_number <= after_sequence_number) {
      continue;
    }

    ++stats.event_count;

    if (event.type == WalEventType::AddOrder) {
      trading::Order order{};
      order.side = event.side;
      order.price = event.price;
      order.quantity = event.quantity;

      // AddOrder assigns the original deterministic order ID based on the
      // order's position in the event sequence.
      auto trades = order_book.AddOrder(std::move(order));

      ++stats.add_order_count;
      stats.generated_trade_count += trades.size();
    } else if (event.type == WalEventType::CancelOrder) {
      ++stats.cancel_order_count;

      if (order_book.CancelOrder(event.order_id)) {
        ++stats.successful_cancel_count;
      }
    } else {
      reader.Close();
      return false;
    }
  }

  reader.Close();

  // The snapshot refers to an event that doesn't exist in the WAL.
  if (last_sequence_number < after_sequence_number) {
    return false;
  }

  return true;
}

}  // namespace recovery
