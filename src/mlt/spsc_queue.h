#ifndef SPSC_QUEUE_H_
#define SPSC_QUEUE_H_
#include <atomic>
#include <vector>
#include "prism/logging.h"

/*!
 * \brief Lamport's single producer single consumer queue
 */
template <typename T>
class SpscQueue {
 public:
  using value_type = T;

  SpscQueue(int count = 32) {
    capacity_ = RoundUpPower2(count);
    CHECK_EQ(capacity_, Lowbit(capacity_)) << "capacity must be power of 2";
    vec_.resize(capacity_);
    mask_ = capacity_ - 1;
    head_.store(0);
    tail_.store(0);
  }

  ~SpscQueue() {}

  void Push(T new_value) {
    /// Notice: when dealing with movable data structure, the commented code below
    /// may cause problems, because once the TryPush fails, the data would be released
    // while (!TryPush(std::forward<T>(new_value))) {
    // }
    auto cur_head = head_.load(std::memory_order_relaxed);
    auto cur_tail = tail_.load(std::memory_order_relaxed);
    while (cur_tail - cur_head >= capacity_) {
      // sched_yield();
      cur_head = head_.load(std::memory_order_relaxed);
      cur_tail = tail_.load(std::memory_order_relaxed);
    }
    vec_[cur_tail & mask_] = std::move(new_value);
    tail_.store(cur_tail + 1, std::memory_order_release);
  }

  void WaitAndPop(T* value) {
    while (!TryPop(value)) {
    }
  }

  bool TryPush(T new_value) {
    auto cur_head = head_.load(std::memory_order_relaxed);
    auto cur_tail = tail_.load(std::memory_order_relaxed);
    if (cur_tail - cur_head >= capacity_) return false;
    vec_[cur_tail & mask_] = std::move(new_value);
    tail_.store(cur_tail + 1, std::memory_order_release);
    return true;
  }

  bool TryPop(T* value) {
    auto cur_head = head_.load(std::memory_order_relaxed);
    auto cur_tail = tail_.load(std::memory_order_acquire);
    if (cur_tail == cur_head) return false;
    *value = std::move(vec_[cur_head & mask_]);
    head_.store(cur_head + 1, std::memory_order_relaxed);
    return true;
  }

 private:
  inline long Lowbit(long x) { return x & -x; }
  long RoundUpPower2(long x) {
    while (x != Lowbit(x)) x += Lowbit(x);
    return x;
  }
  size_t capacity_;
  size_t mask_;
  std::atomic<size_t> head_;
  std::atomic<size_t> tail_;
  std::vector<T> vec_ alignas(32);
};

#endif  // SPSC_QUEUE_H_
