#pragma once

#include <string>

#include "../matching/limit_order_book.h"
#include "../types.h"

namespace recovery {

struct WalReplayStats {
  usize event_count = 0;
  usize add_order_count = 0;
  usize cancel_order_count = 0;
  usize successful_cancel_count = 0;
  usize generated_trade_count = 0;
};

// Replay a WAL into an empty order book.
// Returns false if the WAL cannot be opened or contains an invalid
// record.
//
// If replay fails partway through, the order book may be partially
// rebuilt. The caller should discard it rather than continue using it.

bool32 ReplayWal(const std::string& path,
                 matching::LimitOrderBook& order_book,
                 WalReplayStats& stats,
                 uint64 after_sequence_number = 0);

}  // namespace recovery
