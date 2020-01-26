#ifndef RECEIVING_CHANNEL_H_
#define RECEIVING_CHANNEL_H_

#include "epoll_helper.h"
#include "grad_packet.h"
#include "socket.h"
#include "spsc_queue.h"
#include "thread_proto.h"
#include "ltmessage.h"
#include "threadsafe_queue.h"
// #include "udp_endpoint.h"

class MLTCommunicator;
class ConnMeta;

/**
 * PriorityChannel manages the sockets resources. It contains a reactor running
 * in a seperated thread.
 */
class ReceivingChannel : public TerminableThread {
 public:
  struct Notification {
    Notification() = default;
    enum Type { FINISH_FLOW, CONFIRM_STOP } type;
    union {
      struct {
        int msg_id;
        uint32_t max_seq_num;
        ConnMeta* conn;
      } finish_flow;  // FINISH_FLOW

      struct {
        int msg_id;
        ConnMeta* conn;
      } confirm_stop;  // CONFIRM_STOP
    } data;
  };

  ReceivingChannel(MLTCommunicator* comm, int port, int queue_size = 32);

  virtual ~ReceivingChannel() {}

  virtual void Run();

  void HandleReceive(const char* buf, size_t size);

  void RequestRateAdjustment(int dest, double rx_speed);

  void Enqueue(ConnMeta* conn_meta, const LtMessage& msg, double loss_ratio);

  void Notify(Notification&& notification);

  void PollReceiveRequest();

  void PollNotification();

  void FinishFlow(int msg_id, uint32_t max_seq_num, ConnMeta* conn_meta);

  void ConfirmStop(int msg_id, ConnMeta* conn_meta);

 private:
  /*! \brief: a pointer to MLTCommunicator to access its data */
  MLTCommunicator* comm_;
  /*! \brief: listening port */
  int port_;
  /*! \brief: socket for receiving messages */
  UdpSocket sock_;

  SpscQueue<std::tuple<ConnMeta*, LtMessage, double>> rr_queue_;

  SpscQueue<Notification> notification_queue_;
};

#endif  // RECEIVING_CHANNEL_H_