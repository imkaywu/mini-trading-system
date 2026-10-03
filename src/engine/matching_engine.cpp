#include "matching_engine.h"

#include <stdexcept>

#include "../matching/limit_order_book.cpp"

namespace engine {

MatchingEngine::MatchingEngine(usize expected_order_count)
    : order_book_(expected_order_count) {}

MatchingEngine::~MatchingEngine() { Stop(); }

void MatchingEngine::Start() {
  if (running_) {
    return;
  }

  running_ = true;
  thread_ = std::thread(&MatchingEngine::Run, this);
}

void MatchingEngine::Stop() {
  if (!running_) {
    return;
  }

  running_ = false;

  if (thread_.joinable()) {
    thread_.join();
  }
}

void MatchingEngine::Run() {
  while (running_) {
    // The matching engine will process orders here.
  }
}

}  // namespace engine
