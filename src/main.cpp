#include "matching/limit_order_book.cpp"
#include "matching/limit_order_book.h"
#include "trading/order.h"

int main() {
  matching::LimitOrderBook book;

  const auto first_order_id = book.add_order({
      .side = trading::Side::Buy,
      .price = 100,
      .quantity = 10,
  });

  const auto second_order_id = book.add_order({
      .side = trading::Side::Buy,
      .price = 101,
      .quantity = 20,
  });

  const auto third_order_id = book.add_order({
      .side = trading::Side::Buy,
      .price = 100,
      .quantity = 30,
  });

  const auto fourth_order_id = book.add_order({
      .side = trading::Side::Sell,
      .price = 103,
      .quantity = 15,
  });

  const auto fifth_order_id = book.add_order({
      .side = trading::Side::Sell,
      .price = 102,
      .quantity = 25,
  });

  std::cout << "Initial book:\n";
  book.print();

  std::cout << "\nCancel order " << third_order_id << '\n';

  if (book.cancel_order(third_order_id)) {
    std::cout << "Cancellation succeeded.\n";
  } else {
    std::cout << "Order not found.\n";
  }

  std::cout << "\nBook after cancellation:\n";
  book.print();

  std::cout << "\nCancel order " << third_order_id << " again:\n";

  if (book.cancel_order(third_order_id)) {
    std::cout << "Cancellation succeeded.\n";
  } else {
    std::cout << "Order not found.\n";
  }

  // Keep the other IDs alive so it is obvious that they are
  // still valid orders in the book.
  (void)first_order_id;
  (void)second_order_id;
  (void)fourth_order_id;
  (void)fifth_order_id;

  return 0;
}
