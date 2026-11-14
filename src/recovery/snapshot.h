#pragma once

#include <string>

#include "../matching/limit_order_book.h"
#include "../types.h"

namespace recovery {

// Writes a complete order-book snapshot, replacing any existing file.
bool32 WriteSnapshot(const std::string& path,
                     const matching::LimitOrderBook& order_book);

// Loads a snapshot and restores the order book.
// Returns false if the file is invalid or cannot be read.
bool32 ReadSnapshot(const std::string& path,
                    matching::LimitOrderBook& order_book);
}  // namespace recovery
