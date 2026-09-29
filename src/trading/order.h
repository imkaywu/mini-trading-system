#pragma once

#include <cstdint>

namespace trading {

enum class Side {
  Buy,
  Sell,
};

struct Order {
  std::uint64_t id;
  Side side;
  std::int64_t price;
  std::int64_t quantity;
};

}  // namespace trading
