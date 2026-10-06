#include <chrono>
#include <thread>

#include "engine/spsc_matching_engine.cpp"
#include "matching/limit_order_book.cpp"

int main() {
  engine::SpscMatchingEngine matching_engine(100000);

  matching_engine.Start();

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  matching_engine.Stop();

  return 0;
}
