#include "completion.h" 

int CompletionQueue::PollOnce(int max_comps, Completion* comps) {
  int cnt = 0;
  mu.lock();
  while (cnt < max_comps && !queue.empty()) {
    comps[cnt++] = queue.front();
    queue.pop();
  }
  mu.unlock();
  return cnt;
}

void CompletionQueue::Push(const Completion& comp) {
  std::lock_guard<std::mutex> lk(mu);
  queue.push(comp);
}