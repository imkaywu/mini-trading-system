#include "limit_order_book.h"

#include <iostream>

#include "../types.h"

namespace matching {

std::vector<trading::Trade> LimitOrderBook::add_order(trading::Order order) {
  order.id = next_order_id_++;
  const uint64 order_id = order.id;

  order.timestamp = std::chrono::steady_clock::now();

  std::vector<trading::Trade> trades;

  if (order.side == trading::Side::Buy) {
    while (!asks_.empty()) {
      auto ask_it = asks_.begin();
      const Price ask_price = ask_it->first;

      if (order.price < ask_price) {
        break;
      }

      auto& resting_orders = ask_it->second;
      auto& resting_order = resting_orders.front();

      const int64 fill_quantity =
          std::min(order.quantity, resting_order.quantity);

      trades.push_back({
          .incoming_order_id = order.id,
          .resting_order_id = resting_order.id,
          .price = resting_order.price,
          .quantity = fill_quantity,
      });

      order.quantity -= fill_quantity;
      resting_order.quantity -= fill_quantity;

      if (resting_order.quantity == 0) {
        order_index_.erase(resting_order.id);
        resting_orders.pop_front();
      }

      if (resting_orders.empty()) {
        asks_.erase(ask_it);
      }

      if (order.quantity == 0) {
        return trades;
      }
    }
  } else {
    while (!bids_.empty()) {
      auto bid_it = bids_.begin();
      const Price bid_price = bid_it->first;

      if (order.price > bid_price) {
        break;
      }

      auto& resting_orders = bid_it->second;
      auto& resting_order = resting_orders.front();

      const auto fill_quantity =
          std::min(order.quantity, resting_order.quantity);

      trades.push_back({
          .incoming_order_id = order.id,
          .resting_order_id = resting_order.id,
          .price = resting_order.price,
          .quantity = fill_quantity,
      });

      order.quantity -= fill_quantity;
      resting_order.quantity -= fill_quantity;

      if (resting_order.quantity == 0) {
        order_index_.erase(resting_order.id);
        resting_orders.pop_front();
      }

      if (resting_orders.empty()) {
        bids_.erase(bid_it);
      }

      if (order.quantity == 0) {
        return trades;
      }
    }
  }

  // If the incoming order wasn't fully filled, it becomes a new resting
  // order in the book.
  if (order.quantity > 0) {
    const trading::Side side = order.side;
    const Price price = order.price;
    const uint64 order_id = order.id;

    if (side == trading::Side::Buy) {
      bids_[price].push_back(std::move(order));
    } else {
      asks_[price].push_back(std::move(order));
    }

    order_index_.emplace(order_id,
                         OrderLocation{
                             .side = side,
                             .price = price,
                         });
  }

  return trades;
}

bool32 LimitOrderBook::cancel_order(uint64 order_id) {
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

  for (const auto& [price, orders] : asks_) {
    int64 total_quantity = 0;

    for (const auto& order : orders) {
      const auto timestamp =
          std::chrono::duration_cast<std::chrono::microseconds>(
              order.timestamp.time_since_epoch())
              .count();

      std::cout << "  id=" << order.id << " " << price << " x "
                << order.quantity << " timestamp=" << timestamp << "us\n";

      total_quantity += order.quantity;
    }

    if (orders.size() > 1) {
      std::cout << "  total: " << price << " x " << total_quantity << '\n';
    }
  }

  std::cout << "BIDS:\n";

  for (const auto& [price, orders] : bids_) {
    int64 total_quantity = 0;

    for (const auto& order : orders) {
      const auto timestamp =
          std::chrono::duration_cast<std::chrono::microseconds>(
              order.timestamp.time_since_epoch())
              .count();

      std::cout << "  id=" << order.id << " " << price << " x "
                << order.quantity << " timestamp=" << timestamp << "us\n";

      total_quantity += order.quantity;
    }

    if (orders.size() > 1) {
      std::cout << "  total: " << price << " x " << total_quantity << '\n';
    }
  }

  std::cout << "----------------------\n";
}

}  // namespace matching
