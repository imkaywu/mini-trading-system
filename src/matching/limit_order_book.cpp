#include "limit_order_book.h"

#include <algorithm>
#include <iostream>
#include <utility>
#include <vector>

#include "../types.h"

namespace matching {

LimitOrderBook::LimitOrderBook(usize expected_order_count) {
  if (expected_order_count > 0) {
    order_index_.reserve(expected_order_count);
  }
}

std::vector<trading::Trade> LimitOrderBook::AddOrder(trading::Order order) {
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

bool32 LimitOrderBook::CancelOrder(uint64 order_id) {
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

LimitOrderBookSnapshot LimitOrderBook::ExportSnapshot() const {
  LimitOrderBookSnapshot snapshot;
  snapshot.next_order_id = next_order_id_;

  // Preserve the FIFO order within each price level.
  for (const auto& [price, orders] : bids_) {
    for (const auto& order : orders) {
      snapshot.orders.push_back(order);
    }
  }

  for (const auto& [price, orders] : asks_) {
    for (const auto& order : orders) {
      snapshot.orders.push_back(order);
    }
  }

  return snapshot;
}

bool32 LimitOrderBook::RestoreSnapshot(const LimitOrderBookSnapshot& snapshot) {
  if (snapshot.next_order_id == 0) {
    return false;
  }

  decltype(bids_) bids;
  decltype(asks_) asks;
  decltype(order_index_) order_index;

  order_index.reserve(snapshot.orders.size());

  uint64 max_order_id = 0;

  for (const auto& order : snapshot.orders) {
    if (order.id == 0 || order.quantity <= 0) {
      return false;
    }

    if (order.side != trading::Side::Buy && order.side != trading::Side::Sell) {
      return false;
    }

    if (!order_index
             .emplace(order.id,
                      OrderLocation{
                          .side = order.side,
                          .price = order.price,
                      })
             .second) {
      return false;
    }

    max_order_id = std::max(max_order_id, order.id);

    // Restored timestamps are new runtime timestamps.
    trading::Order restored_order = order;
    restored_order.timestamp = std::chrono::steady_clock::now();

    if (order.side == trading::Side::Buy) {
      bids[order.price].push_back(std::move(restored_order));
    } else {
      asks[order.price].push_back(std::move(restored_order));
    }
  }

  if (snapshot.next_order_id <= max_order_id) {
    return false;
  }

  // Replace the current book only after the snapshot has been validated.
  bids_.swap(bids);
  asks_.swap(asks);
  order_index_.swap(order_index);
  next_order_id_ = snapshot.next_order_id;

  return true;
}

void LimitOrderBook::Print() const {
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
