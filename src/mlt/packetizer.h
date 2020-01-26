#ifndef PACKETIZER_H_
#define PACKETIZER_H_

#include "ltmessage.h"
#include "prio_func.h"
#include "spsc_queue.h"
#include "thread_proto.h"
#include "meter.h"
#include <tuple>

class MLTCommunicator;
class PriorityChannel;

// Attention! Enqueue operation is not thread safe
// User/Caller must ensure
class Packetizer {
 public:
  Packetizer(MLTCommunicator* comm, PriorityChannel* priority_channel)
      : comm_{comm}, priority_channel_{priority_channel} {}
  virtual ~Packetizer() noexcept {}

  uint32_t GetMaxSeqNum(size_t size);

  void PartitionAndRoute(int dest, const LtMessage& msg, PktPrioFunc* prio_func);

  void PartitionOne(GradPacket* grad_pkt, int dest, LtMessageExt& msg_ext,
                    PktPrioFunc* prio_func);

  void PartitionOneBySeq(GradPacket* grad_pkt, int dest,
                         const LtMessageExt& msg, PktPrioFunc* prio_func,
                         int seq);

  size_t GetBytes(const LtMessageExt& msg_ext);

  /**
   * \brief route packet to corresponding priority endpoint
   * 
   * \param grad_pkt packet contains gradients
   * \param is_finished whether it is the final packet of the message
   */
  void RoutePacket(const GradPacket& grad_pkt, bool is_finished);

 private:
  MLTCommunicator* comm_;
  PriorityChannel* priority_channel_;
};

#endif  // PACKETIZER_H_