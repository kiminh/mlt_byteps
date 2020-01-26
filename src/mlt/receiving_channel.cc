#include "receiving_channel.h"
#include "mlt_global.h"
#include "meter.h"
#include "mlt_communicator.h"
#include "completion.h"

ReceivingChannel::ReceivingChannel(MLTCommunicator* comm, int port, int queue_size)
    : comm_{comm}, port_{port}, rr_queue_{queue_size}, notification_queue_{queue_size} {
  AddrInfo ai(port, SOCK_DGRAM);
  sock_.Create(ai);
  sock_.SetReuseAddr(true);
  // sock_.SetReusePort(true);
  sock_.SetNonBlock(true);

  sock_.Bind(ai);
  LOG(INFO) << "MLT bind at address: " << ai.AddrStr();

  // SetSockBuffer();
}

void ReceivingChannel::Enqueue(ConnMeta* conn_meta, const LtMessage& msg,
                               double loss_ratio) {
  rr_queue_.Push({conn_meta, msg, loss_ratio});
}

void ReceivingChannel::Notify(Notification&& notification) {
  notification_queue_.Push(std::move(notification));
}

void ReceivingChannel::Run() {
  int buffer_size = MLTGlobal::Get()->MaxSegment();
  char* buf = new char[buffer_size];

  Meter meter(1000, "receiving_channel");

  while (!terminated_.load()) {
    SockAddr client_addr;
    // TODO(cjr): change to recvmsg, put in backlog buffer first
    ssize_t nbytes = sock_.RecvFrom(buf, buffer_size, 0, &client_addr);

    if (nbytes == -1) {
      CHECK(sock_.LastErrorWouldBlock()) << "errno = " << sock_.GetLastError();
    } else {
      DLOG(TRACE) << "nbytes = " << nbytes;

      meter.Add(nbytes);

      /// handle gradient packets
      HandleReceive(buf, nbytes);
    }

    /// TODO(cjr): cahnge the polling frequency to optimize performance
    /// poll receive requests
    PollReceiveRequest();

    /// poll notification
    PollNotification();
  }

  delete [] buf;
}

void ReceivingChannel::HandleReceive(const char* buf, size_t size) {
  GradPacket* pkt = reinterpret_cast<GradPacket*>(const_cast<char*>(buf));
  pkt->grad_ptr = reinterpret_cast<uint64_t>(buf + kGradPacketHeader);
  DLOG(TRACE) << pkt->DebugString();
  CHECK_EQ(pkt->dst_comm_id, static_cast<uint16_t>(comm_->comm_id()));
  int dest = pkt->src_comm_id;
  int msg_id = pkt->msg_id;

  /// TODO(cjr): what if still receive data after connection has stopped
  comm_->Lock();
  if (comm_->id_conn_.count(dest) == 0 || !comm_->id_conn_[dest]) {
    LOG(WARNING) << "connection has not been established or has been removed";
    comm_->Unlock();
    return;
  }

  ConnMeta* conn_meta = comm_->id_conn_[dest].get();
  comm_->Unlock();

  /// update receiving monitor
  auto& rx_meter = conn_meta->rx_meter;
  rx_meter.Update(size);
  if (rx_meter.Elapsed()) {
    double rx_speed = rx_meter.GetBytesPerSecond();
    RequestRateAdjustment(dest, rx_speed);
    rx_meter.Clear();
  }

  // lock this comm_->id_conn_[dest].mtx;
  // std::lock_guard<std::mutex> lk(conn_meta->mtx);
  LtMessageExt* lt_msg_ext = nullptr;
  auto& recv_msgs_map = conn_meta->recv_msgs;

  auto it = recv_msgs_map.find(msg_id);
  if (it != recv_msgs_map.end()) {
    lt_msg_ext = it->second.get();
  } else {
    auto& free_list = conn_meta->backlog_free_list;
    auto& vec = conn_meta->backlog_used_map[msg_id];
    if (!free_list.empty()) {
      auto& grad_pkt_it = vec.emplace_back(free_list.back());
      free_list.pop_back();
      memcpy(grad_pkt_it, buf, size);
    } else {
      // drop the packet!
    }
  }
  DLOG(TRACE) << "lt_msg_ext = " << lt_msg_ext;

  if (!lt_msg_ext) return;  // recv request not found, so just drop the packet
  size_t copied = lt_msg_ext->CopyGradients(pkt);

  /// TOD(cjr): pay attention of this copied > 0
  if (copied > 0 && lt_msg_ext->FinishReceiving() && !lt_msg_ext->stopped) {
    /// FIXME(cjr): should only execute once
    lt_msg_ext->stopped = true;

    /// 1. send stop request
    auto buffer = std::make_unique<Buffer>(GetOutBufferSize<StopRequest>());
    StopRequest* hdr = GetOutHeader<StopRequest>(buffer.get());
    hdr->type = SignalType::kStopRequest;
    hdr->msg_id = msg_id;
    hdr->comm_id = comm_->comm_id();
    // hdr->sending_rate = ;
    buffer->set_msg_length(buffer->size());
  
    comm_->reliable_channel_->Enqueue(dest, std::move(buffer));
    /// 2. submit to completion queue when receiving kStopConfirm
  }
}

void ReceivingChannel::RequestRateAdjustment(int dest, double rx_speed) {
  auto buffer = std::make_unique<Buffer>(GetOutBufferSize<RateAdjustment>());
  RateAdjustment* hdr = GetOutHeader<RateAdjustment>(buffer.get());
  hdr->type = SignalType::kRateAdjustment;
  hdr->sending_rate = static_cast<float>(rx_speed);
  LOG(TRACE) << "sending RateAdjustment request, receiving_rate: " << rx_speed;

  buffer->set_msg_length(buffer->size());
  comm_->reliable_channel_->Enqueue(dest, std::move(buffer));
}

template <typename T>
inline typename std::enable_if<std::is_integral<T>::value, T>::type AlignUp(
    size_t alignment, T value) {
  return (value + alignment - 1) / alignment * alignment;
}

void ReceivingChannel::PollReceiveRequest() {
  decltype(rr_queue_)::value_type rr;
  while (rr_queue_.TryPop(&rr)) {
    auto [conn_meta, ltmsg, loss_ratio] = std::move(rr);
    int key = ltmsg.msg_id;

    CHECK_EQ(0, conn_meta->recv_msgs.count(key));

    conn_meta->recv_msgs[key] = std::make_unique<LtMessageExt>(ltmsg);

    LtMessageExt* msg_ext = conn_meta->recv_msgs[key].get();

    /// copy backlog message into receive request address
    auto it_vec = conn_meta->backlog_used_map.find(key);
    if (it_vec != conn_meta->backlog_used_map.end() &&
        !it_vec->second.empty()) {
      auto& vec = it_vec->second;
      for (GradPacket* pkt : vec) {
        msg_ext->CopyGradients(pkt);
        conn_meta->backlog_free_list.push_back(pkt);
      }
      vec.clear();
    }

    msg_ext->bound = AlignUp(
        sizeof(float), static_cast<size_t>(ltmsg.size * (1 - loss_ratio)));
    LOG_IF(WARNING, msg_ext->bound == 0) << "message bound is 0";
  }
}

void ReceivingChannel::PollNotification() {
  ReceivingChannel::Notification n;
  while (notification_queue_.TryPop(&n)) {
    switch (n.type) {
      case Notification::FINISH_FLOW: {
        ConnMeta* conn_meta = n.data.finish_flow.conn;
        int msg_id = n.data.finish_flow.msg_id;
        uint32_t max_seq_num = n.data.finish_flow.max_seq_num;
        FinishFlow(msg_id, max_seq_num, conn_meta);
      } break;
      case Notification::CONFIRM_STOP: {
        ConnMeta* conn_meta = n.data.confirm_stop.conn;
        int msg_id = n.data.confirm_stop.msg_id;
        ConfirmStop(msg_id, conn_meta);
      } break;
      default: {
        LOG(FATAL) << "unknown notification type: " << static_cast<int>(n.type);
      }
    }
  }
}

void ReceivingChannel::FinishFlow(int msg_id, uint32_t max_seq_num, ConnMeta* conn_meta) {
  /// 1. check if finish requirement satisfied
  int src_comm_id = conn_meta->dest_comm_id;
  bool finish = false;
  bool found = false;

  /// TODO(cjr): move this to receiving thread, so many locks can be removed
  decltype(LtMessageExt::block_mgr.get()) block_mgr = nullptr;
  auto it = conn_meta->recv_msgs.find(msg_id);
  if (it != conn_meta->recv_msgs.end()) {
    found = true;
    block_mgr = it->second->block_mgr.get();
    finish = it->second->FinishReceiving();
    LOG(TRACE) << "loss_bound = " << it->second->bound
                << " received = " << it->second->bytes_received
                << " src_comm_id = " << conn_meta->dest_comm_id;
  }

  LOG(TRACE) << "msg " << msg_id << " finished: " << finish;

  /// if haven't done yet, send retransmit request
  /// TODO(cjr): check if there is a concurrency issue, yes there is, FIXME(cjr)
  if (!finish) {
    /// request retransmition
    LOG(TRACE) << "max_seq_num = " << max_seq_num << " found = " << found;

    /// because we use tcp stream, so there's no need to packetize
    /// remember std::lock_guard<std::mutex> lk(block_mgr->mtx());
    size_t payload_size = sizeof(Block);
    if (found) {
      if (block_mgr->Size() <= max_seq_num) block_mgr->Resize(max_seq_num + 1);
      payload_size = block_mgr->ByteSize();
    }

    /// dirty hack to avoid lock when check FinishReceiving() above
    if (payload_size == 0) return;

    size_t buffer_size =
        GetOutBufferSize<RetransmitRequest>() + payload_size;
    auto new_buffer =
        std::make_unique<Buffer>(buffer_size);

    /// set header field
    RetransmitRequest* hdr = GetOutHeader<RetransmitRequest>(new_buffer.get());
    hdr->type = SignalType::kRetransmitRequest;
    hdr->msg_id = msg_id;
    hdr->comm_id = comm_->comm_id();
    hdr->num_blocks = payload_size / sizeof(Block);
    if (!found) {
      hdr->blocks[0] = Block(0, max_seq_num + 1);
    } else {
      block_mgr->SerializeToBuffer(hdr->blocks, payload_size);

      LOG(DEBUG) << "src_comm_id: " << src_comm_id
                 << ", hdr->num_blocks: " << hdr->num_blocks
                 << ", loss packets: " << block_mgr->FreeLength();
    }

    /// set buffer sending length and what length that receiver side sees
    new_buffer->set_msg_length(buffer_size);
    RdEndpoint::WriteLength(new_buffer.get());

    comm_->reliable_channel_->Enqueue(src_comm_id, std::move(new_buffer));
  }

  /// if done, nothing to do
  if (finish) {
  }
}

void ReceivingChannel::ConfirmStop(int msg_id, ConnMeta* conn_meta) {
  /// 2. submit to completion queue
  auto& recv_msgs_map = conn_meta->recv_msgs;
  auto it = recv_msgs_map.find(msg_id);
  CHECK(it != recv_msgs_map.end());
  LtMessageExt* lt_msg_ext = it->second.get();

  Completion comp;
  comp.msg_id = msg_id;
  comp.type = CompletionType::kRecv;
  comp.remote_comm_id = conn_meta->dest_comm_id;
  comp.bytes_received = lt_msg_ext->bytes_received;
  comm_->cq_->Push(comp);

  /// 3. remove entry in the map
  recv_msgs_map.erase(it);
}