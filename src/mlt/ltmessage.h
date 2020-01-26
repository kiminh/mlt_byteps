#ifndef LTMESSAGE_H_
#define LTMESSAGE_H_

#include <stddef.h>
#include <stdint.h>
#include "buffer.h"
#include "grad_packet.h"
#include "block_mgr.h"
#include "bitmap_block_mgr.h"
#include "tree_block_mgr.h"

using FlowId = uint64_t;

inline FlowId EncodeFlow(int dest, int msg_id) {
  return (static_cast<FlowId>(dest) << 32) + msg_id;
}

inline std::tuple<int, int> DecodeFlow(FlowId flow_id) {
  return {flow_id >> 32, flow_id & 0xffffffff};
}

// Loss-tolerant message, with an application level message id
// this structure does not take charge of the resource allocation and release
// it's just a view or an pointer
struct LtMessage {
  uint32_t msg_id;
  char* buf;
  size_t size;
};

struct LtMessageExt {
  uint32_t msg_id;
  char* buf;
  size_t size;

  union {
    size_t bytes_received;
    size_t bytes_sent;
  };
  size_t bound;

  bool stopped;

  /// TODO(cjr): this index can be more efficient by using a balanced search
  /// tree, with each node maintaining an interval
  /// use bitset/vector<bool> or set
  std::unique_ptr<BlockMgr> block_mgr;

  LtMessageExt() : block_mgr{std::make_unique<TreeBlockMgr>(0)} {}

  ~LtMessageExt() { block_mgr.reset(); }

  LtMessageExt(const LtMessage& ltmsg)
      : msg_id{ltmsg.msg_id},
        buf{ltmsg.buf},
        size{ltmsg.size},
        bytes_received{0},
        bound{ltmsg.size},
        stopped{false},
        block_mgr{std::make_unique<TreeBlockMgr>(0)} {}

  inline size_t CopyGradients(const GradPacket* pkt) {
    uint32_t seq = pkt->seq;
    /// remember to lock block_mgr
    /// std::lock_guard<std::mutex> lk(block_mgr->mtx());
    if (seq >= block_mgr->Size()) {
      block_mgr->Resize(seq + 1);
    }
    if (block_mgr->Check(seq)) return 0;
    block_mgr->Take(seq);


    uint32_t grad_bytes = pkt->len - kGradPacketHeader;
    CHECK_LE(pkt->offset + grad_bytes, size)
        << "pkt->offset: " << pkt->offset << ", pkt->len: " << pkt->len;
    memcpy(buf + pkt->offset, pkt->GetGradientPtr<float*>(), grad_bytes);
    bytes_received += grad_bytes;

    return grad_bytes;
  }

  inline bool FinishReceiving() { return bytes_received >= bound; }
};

enum class SignalType : uint32_t {
  kUserData,  // user meta data
  kFlowStart,
  kRateAdjustment,
  kFlowFinish,
  kRetransmitRequest,
  kStopRequest,
  kStopConfirm,
};

[[maybe_unused]]
static const char* kSignalTypeStr[] = {
  "kUserData",
  "kFlowStart",
  "kRateAdjustment",
  "kFlowFinish",
  "kRetransmitRequest",
  "kStopRequest",
  "kStopConfirm",
};

struct UserDataHeader {
  SignalType type;
  char payload[0];
};

static_assert(sizeof(UserDataHeader) == 4);

// notify a flow start, the receiver should allocate buffer after receiving this
struct FlowStart {
  SignalType type;
  int msg_id;  // a.k.a msg_id
  uint32_t flow_size;
  uint32_t max_seq_num;
};

static_assert(sizeof(FlowStart) == 16);

struct RateAdjustment {
  SignalType type;
  float sending_rate;
};

static_assert(sizeof(RateAdjustment) == 8);

// I have nothing more to send
struct FlowFinish {
  SignalType type;
  int msg_id;
};

static_assert(sizeof(FlowFinish) == 8);

struct RetransmitRequest {
  SignalType type;
  int msg_id;
  int comm_id;
  // uint32_t num_packets;
  // uint32_t pkt_seqs[0];
  uint32_t num_blocks;
  Block blocks[0];
};

static_assert(sizeof(RetransmitRequest) == 16);

struct StopRequest {
  SignalType type;
  int msg_id;
  int comm_id;
  float sending_rate;
};

static_assert(sizeof(StopRequest) == 16);

// I may have something to send, but you tell me to stop now
struct StopConfirm {
  SignalType type;
  int msg_id;
};

static_assert(sizeof(StopConfirm) == 8);

/// except RetransmitRequest
template <typename T>
inline size_t GetOutBufferSize() {
  return sizeof(T) + sizeof(uint32_t);
}

template <typename T>
inline size_t GetInBufferSize() {
  return sizeof(T);
}

template <typename T>
inline T* GetOutHeader(Buffer* buffer) {
  return reinterpret_cast<T*>(buffer->ptr() + sizeof(uint32_t));
}

template <typename T>
inline T* GetInHeader(Buffer* buffer) {
  return reinterpret_cast<T*>(buffer->ptr());
}

inline SignalType GetTypeFromBuffer(Buffer* buffer) {
  return *reinterpret_cast<SignalType*>(buffer->ptr());
}

#endif  // LTMESSAGE_H_