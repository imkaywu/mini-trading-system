#include "limit_order_book.h"

#include <iostream>

namespace matching {

std::uint64_t LimitOrderBook::add_order(trading::Order order) {
  order.id = next_order_id_++;
  const std::uint64_t order_id = order.id;

  const trading::Side side = order.side;
  const Price price = order.price;

  if (order.side == trading::Side::Buy) {
    bids_[order.price].push_back(std::move(order));
  } else {
    asks_[order.price].push_back(std::move(order));
  }

  order_index_.emplace(order_id,
                       OrderLocation{
                           .side = side,
                           .price = price,
                       });

  return order_id;
}

bool LimitOrderBook::cancel_order(std::uint64_t order_id) {
  const auto index_it = order_index_.find(order_id);

  if (index_it == order_index_.end()) {
    return false;
  }

  const auto [side, price] = index_it->second;

  auto& orders =
      (side == trading::Side::Buy) ? bids_.at(price) : asks_.at(price);

  for (auto it = orders.begin(); it != orders.end(); ++it) {
    if (it->id == order_id) {
      orders.erase(it);
      break;
    }
  }

  order_index_.erase(index_it);

  if (orders.empty()) {
    if (side == trading::Side::Buy) {
      bids_.erase(price);
    } else {
      asks_.erase(price);
    }
  }

  return true;
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
