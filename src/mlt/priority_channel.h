#ifndef PRIORITY_CHANNEL_H_
#define PRIORITY_CHANNEL_H_

#include "epoll_helper.h"
#include "grad_packet.h"
#include "socket.h"
#include "spsc_queue.h"
#include "thread_proto.h"
#include "udp_endpoint.h"
#include "ltmessage.h"
#include "packetizer.h"
#include "conn_meta.h"
#include "threadsafe_queue.h"

#include <unordered_set>

const int kMaxPrio = 0x100;

class MLTCommunicator;

/**
 * PriorityChannel manages the sockets resources. It contains a reactor running
 * in a seperated thread.
 */
class PriorityChannel : public TerminableThread {
 public:
  friend class Packetizer;

  struct Notification {
    Notification() = default;
    enum Type { ADD_CONNECTION, REMOVE_CONNECTION, STOP_FLOW, REQUEST_RETRANSMIT } type;
    union {
      ConnMeta* conn;  // ADD_CONNECTION, REMOVE_CONNECTION
      FlowId flow_id;  // STOP_FLOW
    } data;
    std::unique_ptr<Buffer> req_buffer;  // REQUEST_RETRANSMIT
  };

 public:
  PriorityChannel(MLTCommunicator* comm, int queue_size = 32)
      : comm_{comm},
        epoll_helper_{0},
        sr_queue_{queue_size} {
    std::fill(prio_mapping_.begin(), prio_mapping_.end(), -1);
    packetizer_ = std::make_unique<Packetizer>(comm, this);
  }

  virtual ~PriorityChannel() {}

  virtual void Run();

  void AddEndpoint(UdpEndpoint* endpoint);

  void Enqueue(int dest, const LtMessage& msg, PktPrioFunc* prio_func);

  void Notify(Notification&& notification);

  inline bool HasPrio(int tos) const {
    return 0 <= tos && tos < kMaxPrio && prio_mapping_[tos] != -1;
  }

  void StopFlow(FlowId flow_id);

  inline Packetizer* packetizer() const { return packetizer_.get(); }

  size_t PollSendingMessages();

  size_t PollRetransmitRequest();

  void PollNotification();

 private:
  ConnMeta* FindConnMetaById(int comm_id);

  MLTCommunicator* comm_;
  std::array<ssize_t, kMaxPrio> prio_mapping_;
  /*! \brief: pre-opened UDP sockets for outcoming per-packet QoS */
  std::vector<std::unique_ptr<UdpEndpoint>> prio_endpoints_;
  /*! \brief: epoll helper */
  EpollHelper epoll_helper_;

  ThreadsafeQueue<Notification> notification_queue_;

  SpscQueue<std::tuple<int, std::unique_ptr<LtMessageExt>, PktPrioFunc*>>
      sr_queue_;

  std::vector<ConnMeta*> conn_metas_;
  /// key: comm_id, value: index in conn_metas_
  std::unordered_map<int, size_t> conn_meta_map_;

  std::unique_ptr<Packetizer> packetizer_;

  Meter meter_;
};

#endif  // PRIORITY_CHANNEL_H_