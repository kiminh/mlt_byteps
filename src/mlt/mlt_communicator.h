#ifndef MLT_COMMUNICATOR_H_
#define MLT_COMMUNICATOR_H_

#include "mlt_global.h"
#include "socket.h"
#include "epoll_helper.h"
#include "udp_endpoint.h"
#include "conn_meta.h"
#include "priority_channel.h"
#include "sarray.h"
#include "ltmessage.h"
#include "packetizer.h"
#include "prio_func.h"
#include "buffer.h"
#include "reliable_channel.h"
#include "threadsafe_queue.h"
#include "receiving_channel.h"
#include "completion.h"

#include <vector>
#include <unordered_map>


class MLTCommunicator {
 public:
  friend class Packetizer;
  friend class ReceivingChannel;
  friend class RdEndpoint;
  friend class ReliableChannel;
  friend class PriorityChannel;
  MLTCommunicator(int comm_id, int meta_queue_size = 32)
      : comm_id_{comm_id}, meta_queue_{meta_queue_size} {}

  // initialize the resources, start the threads
  void Start(int listen_port = 0);

  void Finalize();

  void AddConnection(int dest_comm_id, const std::string& host, int port);

  void RemoveConnection(int dest_comm_id);

  void StopUdpReceiving();

  // meta + key + len, these data goes to the reliable channel
  void SendMetaAsync(int dest, Buffer* buffer);

  // this a blocking call, the buffer is created by mlt and released by user
  void RecvMeta(int* dest, Buffer* buffer);

  void PostSend(int dest, const LtMessage& msg, PktPrioFunc* prio_func);

  void PostRecv(int dest, const LtMessage& msg, double loss_ratio);

  void SetCompletionQueue(CompletionQueue* cq) { cq_ = cq; }

  int comm_id() const { return comm_id_; }

  inline void Lock() { mu_.lock(); }
  inline void Unlock() { mu_.unlock(); }

 private:
  /*! \brief: communicator id or rank, inherit from upper layer framework */
  int comm_id_;
  /*! \brief: meta data of all connections, not thread-safe, hash table is
   * faster than lock, so don't worry */
  std::unordered_map<int, std::unique_ptr<ConnMeta>> id_conn_;
  /*! \brief: the queue where receive completions push into */
  CompletionQueue* cq_{nullptr};
  /*! \brief: priority channels */
  std::unique_ptr<PriorityChannel> priority_channel_;
  /*! \brief: reliable channel for control signals and meta data transfer */
  std::unique_ptr<ReliableChannel> reliable_channel_;
  // TODO(cjr): check this potential bottleneck of scalability
  /*! \brief: udp receiving thread */
  std::unique_ptr<ReceivingChannel> receiving_channel_;

  // TODO(cjr): use ThreadsafeQueue
  // TODO(cjr): avoid frequently malloc and free
  // comm_id/dest, buffer
  SpscQueue<std::tuple<int, std::unique_ptr<Buffer>>> meta_queue_;
  std::mutex mu_;
};

#endif  // MLT_COMMUNICATOR_H_
