#ifndef COMPLETION_H_
#define COMPLETION_H_

#include <mutex>
#include <queue>

enum class CompletionType : uint32_t {
  kSend,
  kRecv,
  // kSendMeta,
  // kRecvMeta,
};

struct Completion {
  int msg_id;
  CompletionType type;
  int remote_comm_id;
  union {
    size_t bytes_received;
    size_t bytes_sent;
  };
};

struct CompletionQueue {
  std::mutex mu;
  std::queue<Completion> queue;

  /*!
   * \brief: poll completions
   * 
   * \return number of completions
   */
  int PollOnce(int max_comps, Completion* comps);

  void Push(const Completion& comp);
};

#endif  // COMPLETION_H_