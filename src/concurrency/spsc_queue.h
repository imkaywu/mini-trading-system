#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <utility>

#include "../types.h"

namespace concurrency {

// Single-producer / single-consumer queue.
//
// Contract:
//   - Exactly one thread may call push().
//   - Exactly one different thread may call pop().
//   - The producer owns write_index_.
//   - The consumer owns read_index_.
//
// Violating this contract is a programming error.

template <typename T, usize Capacity>
class SpscQueue {
  static_assert(Capacity > 0);

 public:
  bool32 push(T value) {
    // The producer is the only thread that modifies |write_index_|, so it
    // doesn't need synchronization to read its own index.
    const auto write_index = write_index_.load(std::memory_order_relaxed);

    const auto next_write_index = increment(write_index);

    // We need to observe the consumer's latest read position to determine
    // whether a slot has become available.
    //
    // acquire ensures that we see the consumer's release-store to
    // |read_index_|.
    const auto read_index = read_index_.load(std::memory_order_acquire);

    // We leave one slot unused so that:
    //
    //   read_index == write_index  -> empty
    //
    // Therefore, if advancing write_index would make it
    // equal to read_index, the queue is full.
    if (next_write_index == read_index) {
      return false;
    }

    buffer_[write_index] = std::move(value);

    // Release publishes both:
    //
    //   1. the new write position
    //   2. the Order written above
    //
    // The consumer's acquire load of |write_index_| guarantees that it can
    // safely observe the item in the slot.
    write_index_.store(next_write_index, std::memory_order_release);

    return true;
  }

  bool32 pop(T& value) {
    // The consumer is the only thread that modifies |read_index_|, so it
    // doesn't need synchronization to read its own index.
    const auto read_index = read_index_.load(std::memory_order_relaxed);

    // We need to observe the producer's latest write position.
    //
    // acquire pairs with the producer's release-store to |write_index_|. This
    // guarantees that the Order written into the buffer is visible before we
    // read it.
    const auto write_index = write_index_.load(std::memory_order_acquire);

    if (read_index == write_index) {
      // Queue is empty.
      return false;
    }

    value = std::move(buffer_[read_index]);

    // Publish that this slot is available for reuse by the producer.
    //
    // The producer's acquire load of |read_index_| guarantees that it sees this
    // updated position.
    read_index_.store(increment(read_index), std::memory_order_release);

    return true;
  }

 private:
  static constexpr usize increment(usize index) {
    return (index + 1) % Capacity;
  }

  std::array<T, Capacity> buffer_{};

  /*
   * Producer:
   *  own index     → relaxed
   *  other index   → acquire
   *  publish       → release
   *
   * Consumer:
   *  own index     → relaxed
   *  other index   → acquire
   *  publish       → release
   *
   * Producer writes Order:
   *     │
   *     ▼
   * buffer_[index]
   *     │
   *     ▼
   * release write_index_
   *     │
   *     │ synchronizes-with
   *     ▼
   * acquire write_index_
   *     │
   *     ▼
   * Consumer sees Order
   */
  // Only the producer writes |write_index_|. The consumer reads it.
  std::atomic<int> write_index_;

  // Only the consumer writes |read_index_|. The producer reads it.
  std::atomic<int> read_index_;
};

}  // namespace concurrency
