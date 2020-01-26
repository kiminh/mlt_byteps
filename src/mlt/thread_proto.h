#ifndef THREAD_PROTO_H_
#define THREAD_PROTO_H_
#include "prism/logging.h"

#include <errno.h>
#include <fcntl.h>

#include <atomic>
#include <memory>
#include <thread>

class ThreadProto {
 public:
  virtual ~ThreadProto() noexcept {}

  virtual void Start() {
    this_thread_ = std::make_unique<std::thread>(&ThreadProto::Run, this);
  }

  virtual void Join() {  // NOLINT(*)
    if (this_thread_) this_thread_->join();
  }

  void SetAffinity(int index) {
    return;
    // uint32_t num_cpus = std::thread::hardware_concurrency();
    // cpu_set_t cpuset;
    // CPU_ZERO(&cpuset);
    // CPU_SET(index % num_cpus, &cpuset);

    // int rc = pthread_setaffinity_np(this_thread_->native_handle(),
    //                                 sizeof(cpu_set_t), &cpuset);
    // PCHECK(rc == 0) << "pthread_setaffinity_np failed";
  }

 protected:
  virtual void Run() = 0;

  std::unique_ptr<std::thread> this_thread_;
};

class TerminableThread : public ThreadProto {
 public:
  TerminableThread() : terminated_{false} {}

  virtual void Terminate() {
    terminated_.store(true);
  }

  virtual void Join() override {
    this_thread_->join();
  }

 protected:
  std::atomic<bool> terminated_;
};

class EventThread : public TerminableThread {
 public:
  static int SystemSetNonBlock(int fd, bool non_block) {
    int flags = fcntl(fd, F_GETFL);
    if (non_block) {
      flags |= O_NONBLOCK;
    } else {
      flags &= ~O_NONBLOCK;
    }
    return fcntl(fd, F_SETFL, flags);
  }

  /// a default implementation calling system fcntl
  virtual void SetNonBlocking(bool non_block);
};

#endif  // THREAD_PROTO_H_
