#pragma once

#include <string>

#include "../matching/limit_order_book.h"
#include "../types.h"
#include "wal_replay.h"

namespace recovery {

bool32 RecoverOrderBook(const std::string& snapshot_path,
                        const std::string& wal_path,
                        matching::LimitOrderBook& order_book,
                        WalReplayStats& stats);

}
