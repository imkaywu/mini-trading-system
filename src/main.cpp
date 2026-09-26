#include "matching/limit_order_book.cpp"
#include "matching/limit_order_book.h"
#include "trading/order.h"

int main() {
  matching::LimitOrderBook book;

  book.add_order({
      .side = trading::Side::Buy,
      .price = 100,
      .quantity = 10,
  });

  book.add_order({
      .side = trading::Side::Buy,
      .price = 101,
      .quantity = 20,
  });

  book.add_order({
      .side = trading::Side::Buy,
      .price = 100,
      .quantity = 30,
  });

  book.add_order({
      .side = trading::Side::Sell,
      .price = 103,
      .quantity = 15,
  });

  book.add_order({
      .side = trading::Side::Sell,
      .price = 102,
      .quantity = 25,
  });

  book.print();

  return 0;
}
