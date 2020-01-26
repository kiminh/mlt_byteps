#ifndef THREADSAFE_QUEUE_H_
#define THREADSAFE_QUEUE_H_

#include <mutex>
#include <condition_variable>
#include <queue>

template <typename T>
class ThreadsafeQueue {
 public:
  void Push(T new_value) {
    mu_.lock();
    queue_.push(std::move(new_value));
    mu_.unlock();
    cond_.notify_all();
  }

  void WaitAndPop(T* value) {
    std::unique_lock<std::mutex> lk(mu_);
    cond_.wait(lk, [this] { return !queue_.empty(); });
    *value = std::move(queue_.front());
    queue_.pop();
  }

  bool TryPop(T* value) {
    std::lock_guard<std::mutex> lk(mu_);
    if (queue_.empty()) return false;
    *value = std::move(queue_.front());
    queue_.pop();
    return true;
  }

 private:
  mutable std::mutex mu_;
  std::condition_variable cond_;
  std::queue<T> queue_;
};

#endif  // THREADSAFE_QUEUE_H_