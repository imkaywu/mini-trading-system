#pragma once

#include <array>
#include <mutex>
#include <utility>

#include "../types.h"

namespace concurrency {

// Bounded multiple-producer, single-consumer queue.
//
// Multiple threads may call Push() concurrently.
// Exactly one thread should call Pop().
//
// A mutex protects the ring buffer and both indices.
//
// This is intentionally a simple, correct MPSC baseline,
// not a lock-free implementation.

template <typename T, usize Capacity>
class MpscMutexQueue {
  static_assert(Capacity > 0);

 public:
  // Returns false when the queue is full.
  //
  // On failture, value has not been moved into the buffer.
  bool32 Push(T value) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (count_ == Capacity) {
      return false;
    }

    buffer_[write_index_] = std::move(value);

    write_index_ = Increment(write_index_);
    ++count_;

    return true;
  }

  // Returns false when the queue is empty.
  //
  // Only one cnosumer should call this method.
  bool32 Pop(T& value) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (count_ == 0) {
      return false;
    }

    value = std::move(buffer_[read_index_]);

    read_index_ = Increment(read_index_);
    --count_;

    return true;
  }

 private:
  static constexpr usize Increment(usize index) {
    return (index + 1) % Capacity;
  }

  std::array<T, Capacity> buffer_{};

  usize write_index_ = 0;
  usize read_index_ = 0;
  usize count_ = 0;

  // Mutable is unnecessary because neither Push or Pop is const.
  std::mutex mutex_;
};

}  // namespace concurrency
