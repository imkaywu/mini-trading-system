#pragma once

#include <chrono>

#include "../types.h"

namespace trading {

using Timestamp = std::chrono::steady_clock::time_point;

enum class Side {
  Buy,
  Sell,
};

struct Order {
  uint64 id;
  Side side;
  int64 price;
  int64 quantity;
  Timestamp timestamp;  // Time when order entered the matching engine.
};

}  // namespace trading
