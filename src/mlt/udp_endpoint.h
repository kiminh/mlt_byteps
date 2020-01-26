#ifndef UDP_ENDPOINT_H_
#define UDP_ENDPOINT_H_

#include "epoll_helper.h"
#include "grad_packet.h"
#include "socket.h"

#include <queue>

class PriorityChannel;

class UdpEndpoint {
 public:
  using TxQueue = std::queue<GradPacket>;

  UdpEndpoint(int tos);

  ~UdpEndpoint();

  ssize_t OnSendReady();

  inline int fd() const { return sock_; }

  inline const UdpSocket& sock() const { return sock_; }
  inline UdpSocket& sock() { return sock_; }

  inline const struct epoll_event& event() const { return event_; }
  inline struct epoll_event& event() { return event_; }

  inline int tos() const { return tos_; }

  inline void set_prio_channel(PriorityChannel* prio_channel) {
    prio_channel_ = prio_channel;
  }

  inline TxQueue& tx_queue() { return tx_queue_; }

 private:
  int tos_;
  UdpSocket sock_;
  struct epoll_event event_;
  PriorityChannel* prio_channel_;
  TxQueue tx_queue_;
};

#endif  // UDP_ENDPOINT_H_