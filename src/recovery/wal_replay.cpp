#include "wal_replay.h"

#include <utility>

#include "write_ahead_log.h"

namespace recovery {

bool32 ReplayWal(const std::string& path,
                 matching::LimitOrderBook& order_book,
                 WalReplayStats& stats) {
  stats = {};

  WalReader reader;

  if (!reader.Open(path)) {
    return false;
  }

  WalEvent event{};

  while (true) {
    const WalReadResult result = reader.ReadNext(event);

    if (result == WalReadResult::End) {
      reader.Close();
      return true;
    }

    if (result == WalReadResult::Error) {
      reader.Close();
      return false;
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
}

}  // namespace recovery
