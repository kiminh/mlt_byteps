#include "packetizer.h"
#include "prism/utils.h"
#include "mlt_global.h"
#include "mlt_communicator.h"
#include "buffer.h"
#include "priority_channel.h"

uint32_t Packetizer::GetMaxSeqNum(size_t size) {
  size_t bound = MLTGlobal::Get()->MaxSegment() - kGradPacketHeader;
  return (size + bound - 1) / bound - 1;   // number of packets - 1
}

void Packetizer::RoutePacket(const GradPacket& pkt, bool is_finished) {
  UdpEndpoint* endpoint =
      priority_channel_
          ->prio_endpoints_[priority_channel_->prio_mapping_[pkt.tos]]
          .get();
  endpoint->tx_queue().push(pkt);

  DLOG(TRACE) << "sending pkt: " << pkt.DebugString();

  if (is_finished) {
    /// 1. send flow finish notification
    /// TODO(cjr): remove the overhead of sending signals that are typically small messages
    auto buffer = std::make_unique<Buffer>(GetOutBufferSize<FlowFinish>());
    FlowFinish* hdr = GetOutHeader<FlowFinish>(buffer.get());
    hdr->type = SignalType::kFlowFinish;
    hdr->msg_id = pkt.msg_id;
    buffer->set_msg_length(buffer->size());
    comm_->reliable_channel_->Enqueue(pkt.dst_comm_id, std::move(buffer));
  }
}

void Packetizer::PartitionAndRoute(int dest, const LtMessage& msg, PktPrioFunc* prio_func) {
  auto size = msg.size;
  size_t bound = MLTGlobal::Get()->MaxSegment() - kGradPacketHeader;
  size_t accumulated = 0;
  uint32_t seq = 0;
  auto start = std::chrono::high_resolution_clock::now();
  while (accumulated < size) {
    // auto start1 = std::chrono::high_resolution_clock::now();
    GradPacket pkt;
    pkt.msg_id = msg.msg_id;
    pkt.offset = accumulated;
    pkt.len = ((size - accumulated) > bound ? bound : (size - accumulated)) +
              kGradPacketHeader;
    pkt.seq = seq++;
    pkt.dst_comm_id = dest;
    pkt.src_comm_id = comm_->comm_id();
    pkt.is_last = (accumulated + pkt.len - kGradPacketHeader == size) ? 1 : 0;
    pkt.grad_ptr = reinterpret_cast<uint64_t>(msg.buf) + accumulated;
    // pkt.tos = (*prio_func)(pkt);
    pkt.tos = rand() % 256;
    // auto end1 = std::chrono::high_resolution_clock::now();
    // printf("round duration: %.3fus\n", (end1 - start1).count() / 1e3);
    // LOG(TRACE) << pkt.DebugString();
    RoutePacket(pkt, pkt.is_last == 1 ? true : false);
    accumulated += pkt.len - kGradPacketHeader;
  }
  auto end = std::chrono::high_resolution_clock::now();
  LOG(INFO) << prism::FormatString("msg.size = %ld, duration: %.3fus", msg.size,
                                   (end - start).count() / 1e3);
}

size_t Packetizer::GetBytes(const LtMessageExt& msg_ext) {
  auto size = msg_ext.size;
  size_t bound = MLTGlobal::Get()->MaxSegment() - kGradPacketHeader;
  size_t accumulated = msg_ext.bytes_sent;
  return ((size - accumulated) > bound ? bound : (size - accumulated)) +
         kGradPacketHeader;
}

void Packetizer::PartitionOne(GradPacket* grad_pkt, int dest,
                              LtMessageExt& msg_ext, PktPrioFunc* prio_func) {
  auto size = msg_ext.size;
  size_t bound = MLTGlobal::Get()->MaxSegment() - kGradPacketHeader;
  size_t& accumulated = msg_ext.bytes_sent;
  uint32_t seq = accumulated / bound;

  GradPacket& pkt = *grad_pkt;
  pkt.msg_id = msg_ext.msg_id;
  pkt.offset = accumulated;
  pkt.len = ((size - accumulated) > bound ? bound : (size - accumulated)) +
            kGradPacketHeader;
  pkt.seq = seq;
  pkt.dst_comm_id = dest;
  pkt.src_comm_id = comm_->comm_id();
  pkt.is_last = (accumulated + pkt.len - kGradPacketHeader == size) ? 1 : 0;
  pkt.grad_ptr = reinterpret_cast<uint64_t>(msg_ext.buf) + accumulated;
  pkt.tos = (*prio_func)(pkt);
  // LOG(TRACE) << pkt.DebugString();
  accumulated += pkt.len - kGradPacketHeader;
}

void Packetizer::PartitionOneBySeq(GradPacket* grad_pkt, int dest,
                                   const LtMessageExt& msg,
                                   PktPrioFunc* prio_func, int seq) {
  auto size = msg.size;
  size_t bound = MLTGlobal::Get()->MaxSegment() - kGradPacketHeader;
  size_t offset = bound * seq;

  GradPacket& pkt = *grad_pkt;
  pkt.msg_id = msg.msg_id;
  pkt.offset = offset;
  pkt.len =
      ((size - offset) > bound ? bound : (size - offset)) + kGradPacketHeader;
  pkt.seq = seq;
  pkt.dst_comm_id = dest;
  pkt.src_comm_id = comm_->comm_id();
  pkt.is_last = (offset + pkt.len - kGradPacketHeader == size) ? 1 : 0;
  pkt.grad_ptr = reinterpret_cast<uint64_t>(msg.buf) + offset;
  pkt.tos = (*prio_func)(pkt);
  // LOG(TRACE) << "retarnsmit:" << pkt.DebugString();
}