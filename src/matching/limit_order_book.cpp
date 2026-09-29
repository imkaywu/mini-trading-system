#include "limit_order_book.h"

#include <iostream>

namespace matching {

std::uint64_t LimitOrderBook::add_order(trading::Order order) {
  order.id = next_order_id_++;
  const std::uint64_t order_id = order.id;

  if (order.side == trading::Side::Buy) {
    bids_[order.price].push_back(std::move(order));
  } else {
    asks_[order.price].push_back(std::move(order));
  }

  return order_id;
}

void LimitOrderBook::print() const {
  std::cout << "----- ORDER BOOK -----\n";

  std::cout << "ASKS:\n";

  // asks_ is already sorted from lowest price to highest price.
  for (const auto& [price, orders] : asks_) {
    std::int64_t total_quantity = 0;

    for (const auto& order : orders) {
      total_quantity += order.quantity;
    }

    std::cout << "  " << price << " x " << total_quantity << "\n";
  }

  std::cout << "BIDS:\n";

  // bids_ is sorted from highest price to lowest price.
  for (const auto& [price, orders] : bids_) {
    std::int64_t total_quantity = 0;

    for (const auto& order : orders) {
      total_quantity += order.quantity;
    }

    std::cout << "  " << price << " x " << total_quantity << "\n";
  }

  std::cout << "----------------------\n";
}

}  // namespace matching
