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

  std::cout << "Order IDs:\n";
  std::cout << "  first  = " << first_order_id << '\n';
  std::cout << "  second = " << second_order_id << '\n';
  std::cout << "  third  = " << third_order_id << '\n';
  std::cout << "  fourth = " << fourth_order_id << '\n';
  std::cout << "  fifth  = " << fifth_order_id << '\n';

  book.print();

  return 0;
}
