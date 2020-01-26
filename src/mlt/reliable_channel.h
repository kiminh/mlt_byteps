#ifndef RELIABLE_CHANNEL_H_
#define RELIABLE_CHANNEL_H_

#include <unordered_map>
#include "epoll_helper.h"
#include "rd_endpoint.h"
#include "socket.h"
#include "spsc_queue.h"
#include "threadsafe_queue.h"
#include "thread_proto.h"

class MLTCommunicator;

class ReliableChannel : public TerminableThread {
 public:
  // enum class NotificationType {
  //   kAddEndpoint,
  //   kRemoveEndpoint,
  // };

  struct Notification {
    enum Type { ADD_ENDPOINT, REMOVE_ENDPOINT } type;
    union {
      int dest_comm_id;  // REMOVE_ENDPOINT
    } data;
    std::shared_ptr<RdEndpoint> endpoint;  // ADD_ENDPOINT
  };

  ReliableChannel(MLTCommunicator* comm, int queue_size)
      : comm_{comm}, epoll_helper_{0} {}

  virtual ~ReliableChannel() noexcept;

  virtual void Run();

  void Enqueue(int dest, Buffer* buffer);

  void Enqueue(int dest, std::unique_ptr<Buffer> buffer);

  void Notify(Notification&& notification);

  void AddEndpoint(std::shared_ptr<RdEndpoint> endpoint);

  void RemoveEndpoint(int dest);

  void HandleNewConnection();

  void Listen(int port);

 private:
  MLTCommunicator* comm_;
  /*! \brief: a listening socket for establishing connection */
  TcpSocket listening_sock_;
  int listen_port_;
  struct epoll_event listen_event_;
  /*! \brief: control sockets of each connection */
  std::unordered_map<int, std::shared_ptr<RdEndpoint>> ctrl_endpoints_;
  /*! \brief: epoll helper */
  EpollHelper epoll_helper_;
  // route packet to corresponding endpoint
  // SpscQueue<std::tuple<int, std::unique_ptr<Buffer>>> tx_queue_;
  // accessed by user thread and packetize thread
  ThreadsafeQueue<std::tuple<int, std::unique_ptr<Buffer>>> tx_queue_;

  SpscQueue<ReliableChannel::Notification> notification_queue_;
};

#endif  // RELIABLE_CHANNEL_H_