#include <iostream>

#include "matching/limit_order_book.cpp"
#include "matching/limit_order_book.h"
#include "trading/order.h"
#include "trading/trade.h"

void print_trades(const std::vector<trading::Trade>& trades) {
  for (const auto& trade : trades) {
    std::cout << "Trade: incoming=" << trade.incoming_order_id
              << " resting=" << trade.resting_order_id
              << " price=" << trade.price << " quantity=" << trade.quantity
              << '\n';
  }
}

int main() {
  matching::LimitOrderBook book;

  // Resting sell order.
  const auto sell_id = book.add_order({
      .id = 0,
      .side = trading::Side::Sell,
      .price = 100,
      .quantity = 50,
  });

  std::cout << "Initial book:\n";
  book.print();

  // Partially fills the resting sell order.
  //
  // SELL 100 x 50
  // BUY  100 x 20
  //
  // Result:
  // SELL 100 x 30
  const auto trades = book.add_order({
      .id = 0,
      .side = trading::Side::Buy,
      .price = 100,
      .quantity = 20,
  });

  std::cout << "\nAfter partial fill:\n";
  print_trades(trades);
  book.print();

  // This buy order consumes the remaining 30 shares.
  const auto second_trades = book.add_order({
      .id = 0,
      .side = trading::Side::Buy,
      .price = 100,
      .quantity = 40,
  });

  std::cout << "\nAfter consuming the remaining sell:\n";
  print_trades(second_trades);
  book.print();

  // The incoming sell cannot cross the empty book,
  // so it becomes a resting order.
  const auto resting_sell_id = book.add_order({
      .id = 0,
      .side = trading::Side::Sell,
      .price = 103,
      .quantity = 30,
  });

  // This buy crosses the sell at 103 and has quantity left over.
  //
  // SELL 103 x 30
  // BUY  103 x 50
  //
  // Result:
  // SELL → completely filled
  // BUY  → 20 remaining and becomes a resting bid
  const auto third_trades = book.add_order({
      .id = 0,
      .side = trading::Side::Buy,
      .price = 103,
      .quantity = 50,
  });

  std::cout << "\nAfter partial fill of incoming order:\n";
  print_trades(third_trades);
  book.print();

  // These variables are kept here so the IDs can be inspected
  // while debugging this stage.
  (void)sell_id;
  (void)resting_sell_id;

  return 0;
}
