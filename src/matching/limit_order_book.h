#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <map>

#include "../trading/order.h"

namespace matching {

class LimitOrderBook {
 public:
  void add_order(const trading::Order& order);

  void print() const;

 private:
  using Price = std::int64_t;

  using OrderQueue = std::deque<trading::Order>;

  // Bids:
  // Highest price has priority, so prices are sorted descending.
  std::map<Price, OrderQueue, std::greater<Price>> bids_;

  // Asks:
  // Lowest price has priority, so prices are sorted ascending.
  std::map<Price, OrderQueue> asks_;
};

}  // namespace matching
