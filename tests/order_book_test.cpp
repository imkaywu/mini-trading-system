#include <iostream>

#include "../src/matching/limit_order_book.cpp"
#include "../src/trading/order.h"
#include "../src/trading/trade.h"

void PrintTrades(const std::vector<trading::Trade>& trades) {
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
  const auto sell_id = book.AddOrder({
      .id = 0,
      .side = trading::Side::Sell,
      .price = 100,
      .quantity = 50,
      .timestamp = {},
  });

  std::cout << "Initial book:\n";
  book.Print();

  // Partially fills the resting sell order.
  //
  // SELL 100 x 50
  // BUY  100 x 20
  //
  // Result:
  // SELL 100 x 30
  const auto trades = book.AddOrder({
      .id = 0,
      .side = trading::Side::Buy,
      .price = 100,
      .quantity = 20,
      .timestamp = {},
  });

  std::cout << "\nAfter partial fill:\n";
  PrintTrades(trades);
  book.Print();

  // This buy order consumes the remaining 30 shares.
  const auto second_trades = book.AddOrder({
      .id = 0,
      .side = trading::Side::Buy,
      .price = 100,
      .quantity = 40,
      .timestamp = {},
  });

  std::cout << "\nAfter consuming the remaining sell:\n";
  PrintTrades(second_trades);
  book.Print();

  // The incoming sell cannot cross the empty book,
  // so it becomes a resting order.
  const auto resting_sell_id = book.AddOrder({
      .id = 0,
      .side = trading::Side::Sell,
      .price = 103,
      .quantity = 30,
      .timestamp = {},
  });

  // This buy crosses the sell at 103 and has quantity left over.
  //
  // SELL 103 x 30
  // BUY  103 x 50
  //
  // Result:
  // SELL → completely filled
  // BUY  → 20 remaining and becomes a resting bid
  const auto third_trades = book.AddOrder({
      .id = 0,
      .side = trading::Side::Buy,
      .price = 103,
      .quantity = 50,
      .timestamp = {},
  });

  std::cout << "\nAfter partial fill of incoming order:\n";
  PrintTrades(third_trades);
  book.Print();

  // These variables are kept here so the IDs can be inspected
  // while debugging this stage.
  (void)sell_id;
  (void)resting_sell_id;

  return 0;
}
