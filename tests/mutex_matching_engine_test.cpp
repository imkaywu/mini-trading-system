#include <chrono>
#include <thread>

#include "../src/engine/mutex_matching_engine.cpp"

int main() {
  engine::MutexMatchingEngine matching_engine(100000);

  matching_engine.Start();

  matching_engine.SubmitOrder({
      .id = 0,
      .side = trading::Side::Buy,
      .price = 100,
      .quantity = 10,
  });

  matching_engine.SubmitOrder({
      .id = 0,
      .side = trading::Side::Sell,
      .price = 100,
      .quantity = 5,
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  matching_engine.Stop();

  return 0;
}
