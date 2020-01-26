#ifndef RD_ENDPOINT_H_
#define RD_ENDPOINT_H_

#include "socket.h"
#include "epoll_helper.h"
#include "sarray.h"
#include "buffer.h"
#include "grad_packet.h"
#include "conn_meta.h"

#include <queue>
#include <unordered_map>

class MLTCommunicator;

/// Reliable datagram endpoint, currently use TCP for reliability
class RdEndpoint {
 public:
  using TxQueue = std::queue<std::unique_ptr<Buffer>>;

  RdEndpoint(int tos, ConnMeta* conn_meta);

  // construct from existing socket
  RdEndpoint(int tos, TcpSocket new_sock, ConnMeta* conn_meta);

  ~RdEndpoint();

  void Connect();

  void Disconnect();

  void OnConnected();

  void OnAccepted();

  void OnSendReady();

  void OnRecvReady();

  void OnError();

  void HandleReceivedData(std::unique_ptr<Buffer> buffer);

  static inline void WriteLength(Buffer* buffer) {
    ssize_t value = buffer->size() - sizeof(uint32_t);
    CHECK_GE(value, 0);
    *reinterpret_cast<uint32_t*>(buffer->ptr()) = value;
  }

  inline int comm_id() const { return conn_meta_->dest_comm_id; }

  inline int fd() const { return sock_; }

  inline const TcpSocket& sock() const { return sock_; }
  inline TcpSocket& sock() { return sock_; }

  inline const struct epoll_event& event() const { return event_; }
  inline struct epoll_event& event() { return event_; }

  inline void set_dead(bool is_dead) { is_dead_ = is_dead; }
  inline bool is_dead() const { return is_dead_; }

  inline TxQueue& tx_queue() { return tx_queue_; }

  inline void set_comm(MLTCommunicator* comm) { comm_ = comm; }

 private:
  int tos_;
  TcpSocket sock_;
  ConnMeta* conn_meta_;
  struct epoll_event event_;
  bool is_dead_ {false};
  TxQueue tx_queue_;
  std::vector<std::unique_ptr<Buffer>> rx_buffers_;

  MLTCommunicator* comm_;

  /// TODO(cjr): decide who maintains this
  /// key: msg_id, value: max_seq_num
  std::unordered_map<int, uint32_t> msg_max_seq;
};

#endif  // RD_ENDPOINT_H_