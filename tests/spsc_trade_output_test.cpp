#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>

#include "../src/engine/spsc_matching_engine.cpp"
#include "../src/matching/limit_order_book.cpp"

int main() {
  std::vector<trading::Trade> received_trades;

  engine::SpscMatchingEngine engine(100, [&](const trading::Trade& trade) {
    received_trades.push_back(trade);
  });

  engine.Start();

  // Resting buy: 10 units at price 100.
  trading::Order buy{};
  buy.side = trading::Side::Buy;
  buy.price = 100;
  buy.quantity = 10;

  // Incoming sell: matches 4 units.
  trading::Order sell1{};
  sell1.side = trading::Side::Sell;
  sell1.price = 100;
  sell1.quantity = 4;

  // Incoming sell: matches the remaining 6 units.
  trading::Order sell2{};
  sell2.side = trading::Side::Sell;
  sell2.price = 100;
  sell2.quantity = 6;

  // Retry when the bounded input queue is full.
  auto submit = [&](trading::Order order) {
    while (!engine.SubmitOrder(order)) {
      std::this_thread::yield();
    }
  };

  submit(buy);
  submit(sell1);
  submit(sell2);

  // Stop() must drain accepted orders and generated trades.
  engine.Stop();

  // All three orders were processed.
  assert(engine.ProcessedCount() == 3);

  // Exactly two trades were generated and consumed.
  assert(engine.ConsumedTradeCount() == 2);
  assert(received_trades.size() == 2);

  // FIFO trade ordering.
  assert(received_trades[0].price == 100);
  assert(received_trades[0].quantity == 4);

  assert(received_trades[1].price == 100);
  assert(received_trades[1].quantity == 6);

  // Calling Stop() again must be safe.
  engine.Stop();

  std::printf("Trade output test passed\n");

  return 0;
}
