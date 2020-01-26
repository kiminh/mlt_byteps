#ifndef CONN_META_H_
#define CONN_META_H_

#include "socket.h"
#include "buffer.h"
#include "ltmessage.h"
#include "prio_func.h"
#include "meter.h"

#include <atomic>
#include <map>
#include <unordered_map>
#include <mutex>

/**
 *  \brief: Connection meta information, this is not thread-safe.
 *
 * This data structure is accessed by user thread, receiving thread and reliable
 * channel thread.
 */
struct ConnMeta {
  int dest_comm_id;
  // SockAddr remote_addr;
  // TcpSocket ctrl_sock;  // DONT WAIT
  // std::shared_ptr<RdEndpoint> ctrl_endpoint;
  // sending window, unit: bytes
  std::atomic<size_t> send_window;
  std::atomic<double> sending_rate;

  using SendRequest = std::tuple<std::unique_ptr<LtMessageExt>, PktPrioFunc*>;
  // key: msg_id, value <LtMessageExt, PktPrioFunc*>
  std::map<int, SendRequest> sending_msgs;
  std::map<int, SendRequest> retransmitting_msgs;
  // key: msg_id, value buffer with type kRetransmitRequest, current index in pkt_seqs
  struct RetransmitState {
    uint32_t block_num;
    uint32_t seq_num;
    RetransmitState() : block_num{0}, seq_num{0} {}
    RetransmitState(uint32_t bn, uint32_t sn) : block_num{bn}, seq_num{sn} {}
  };
  std::map<int, std::tuple<std::unique_ptr<Buffer>, RetransmitState>> retransmit_reqs;
  // TODO(cjr): use timestamp as key instead of msg_id

  // sending rate monitor
  RateMeter tx_meter;
  // receiving rate monitor
  RateMeter rx_meter;

  std::mutex mtx;
  // key: msg_id, value: LtMessageExt
  std::unordered_map<int, std::unique_ptr<LtMessageExt>> recv_msgs;
  // the size should large than BDP * 1 instead of BDP * num_connections
  // these members are only accessed by receiving thread
  size_t backlog_buffer_size;
  std::unique_ptr<char[]> backlog_buffer;
  std::vector<GradPacket*> backlog_free_list;
  std::unordered_map<int, std::vector<GradPacket*>> backlog_used_map;

  ConnMeta(int dest);

  void InitBacklog();
};

#endif  //  CONN_META_H_