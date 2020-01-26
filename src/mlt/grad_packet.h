#ifndef GRAD_PACKET_H_
#define GRAD_PACKET_H_

#include <stdint.h>
#include <sstream>

#define PACKED __attribute__((__packed__))

// TODO(cjr): compress this header
struct GradMessage {
  uint32_t msg_id;      // a.k.a. tensor_id, get from the application
  uint32_t offset;       // offset in the origin tensor
  uint32_t seq;          // chunk_id or packet sequence number, for retransmition
  uint16_t len;          // bytes of the whole message including header
  uint16_t dst_comm_id;  // dst communicator id
  uint16_t src_comm_id;  // src communicator id
  uint8_t tos;
  uint8_t is_last;       // whether it is the last packet of the flow
  uint64_t grad_ptr;

  /// Attention: this function is very slow, should not occur in datapath
  inline std::string DebugString() const {
    std::stringstream ss;
    ss << "{ msg_id: " << msg_id
       << ", offset: " << offset
       << ", seq: " << seq
       << ", len: " << len
       << ", dst_comm_id: " << dst_comm_id
       << ", src_comm_id: " << src_comm_id
       << ", tos: " << static_cast<int>(tos)
       << ", is_last: " << static_cast<bool>(is_last) << " }";
    return ss.str();
  }

  template <typename T>
  inline T* GetGradientPtr() const {
    return reinterpret_cast<T*>(this->grad_ptr);
  }

  // template <typename T>
  // inline int GetNumGradients() const {
  //   CHECK((len - kGradPacketHeader) % sizeof(T) == 0);
  //   return (len - kGradPacketHeader) / sizeof(T);
  // }
} PACKED;

/// only when the length of the message is less than MTU, the message is in a
/// packet
using GradPacket = GradMessage;

const int kGradPacketHeader = sizeof(GradPacket) - sizeof(GradMessage::grad_ptr);

static_assert(kGradPacketHeader == 20);

template <typename T>
inline T* GetGradientPtr(const GradPacket& pkt) {
  return reinterpret_cast<T*>(pkt.grad_ptr);
}

template <typename T>
inline int GetNumGradients(const GradPacket& pkt) {
  CHECK((pkt.len - kGradPacketHeader) % sizeof(T) == 0);
  return (pkt.len - kGradPacketHeader) / sizeof(T);
}

#endif  // GRAD_PACKET_H_
