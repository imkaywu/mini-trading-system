#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

#include "../types.h"

namespace concurrency {

template <typename T>
class MutexQueue {
 public:
  void Push(T value) {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.push(std::move(value));
    }
    condition_variable_.notify_one();
  }

  bool Pop(T& value) {
    std::unique_lock<std::mutex> lock(mutex_);

    condition_variable_.wait(lock,
                             [this] { return !queue_.empty() || !running_; });

    if (queue_.empty()) {
      return false;
    }

    value = std::move(queue_.front());
    queue_.pop();

    return true;
  }

  void Stop() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      running_ = false;
    }

    condition_variable_.notify_all();
  }

 private:
  std::queue<T> queue_;

  std::mutex mutex_;
  std::condition_variable condition_variable_;

  bool32 running_ = true;
};

}  // namespace concurrency
