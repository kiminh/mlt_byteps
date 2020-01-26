#include "rd_endpoint.h"
#include "mlt_global.h"
#include "ltmessage.h"
#include "conn_meta.h"
#include "completion.h"
#include "mlt_communicator.h"

#include <chrono>
#include <thread>
#include <algorithm>

RdEndpoint::RdEndpoint(int tos, ConnMeta* conn_meta)
    : tos_{tos}, conn_meta_{conn_meta} {
  sock_.Create();
  sock_.SetTos(tos);

  event_.events = EPOLLIN | EPOLLOUT | EPOLLERR;
  event_.data.ptr = this;
}

RdEndpoint::RdEndpoint(int tos, TcpSocket new_sock, ConnMeta* conn_meta)
    : tos_{tos}, sock_{new_sock}, conn_meta_{conn_meta} {
  sock_.SetTos(tos);

  event_.events = EPOLLIN | EPOLLOUT | EPOLLERR;
  event_.data.ptr = this;
}

RdEndpoint::~RdEndpoint() {
  if (!sock_.IsClosed()) sock_.Close();
}

void RdEndpoint::Connect() {
  const SockAddr& addr = MLTGlobal::Get()->AddrFromCommId(comm_id());
  while (!sock_.Connect(addr)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
}

void RdEndpoint::Disconnect() {
  sock_.Close();
  set_dead(true);
}

void RdEndpoint::OnAccepted() {
  sock_.SetNonBlock(true);
  LOG(INFO) << "reliable channel from comm_id " << comm_id() << " accepted";
  // change state to connected
}

void RdEndpoint::OnConnected() {
  sock_.SetNonBlock(true);
  LOG(INFO) << "reliable channel to comm_id " << comm_id() << " connected";
  // change state to connected
}

// TODO(cjr): poll out all tx elements
void RdEndpoint::OnSendReady() {
  // static int cnt = 0;
  while (!tx_queue_.empty()) {
    // printf("tx_queue_ size = %ld\n", tx_queue_.size());
    Buffer* buffer = tx_queue_.front().get();

    if (buffer->IsClear()) {
      tx_queue_.pop();
      continue;
    }

    if (buffer->GetRemainBuffer() == buffer->ptr()) {
      DLOG(TRACE) << "send " << kSignalTypeStr[static_cast<int>(*(int*)(buffer->ptr() + 4))];
    }

    ssize_t nbytes = sock_.Send(buffer->GetRemainBuffer(), buffer->GetRemainSize());
    // if (buffer->GetRemainBuffer() == buffer->ptr())
    //   DLOG(TRACE) << "send " << cnt++ << " " << nbytes << " "
    //              << *reinterpret_cast<uint32_t*>(buffer->ptr());
    if (nbytes == -1) {
      CHECK(sock_.LastErrorWouldBlock()) << "errno = " << sock_.GetLastError();
      // cannot send anymore
      break;
    }
    buffer->MarkHandled(nbytes);
    if (buffer->IsClear()) {
      tx_queue_.pop();
    }
  }
}

void RdEndpoint::OnRecvReady() {
  // static int cnt = 0;
  while (1) {
    bool found = false;
    std::unique_ptr<Buffer> buffer;

    while (!rx_buffers_.empty()) {
      buffer = std::move(rx_buffers_.back());
      // DLOG(TRACE) << "get buffer from rx_buffers";
      rx_buffers_.pop_back();
      if (!buffer->IsClear()) {
        found = true;
        break;
      }
    }

    if (!found) {
      uint32_t length;

      int ret = sock_.Recv(&length, sizeof(length));
      if (ret == -1) {
        CHECK(sock_.LastErrorWouldBlock()) << "errno = " << sock_.GetLastError();
        return;
      } else if (ret == 0) {
        LOG(WARNING) << "peer " << comm_id() << " has shutdown, disconnecting...";
        Disconnect();
        return;
      }

      // DLOG(TRACE) << "recv length, cnt = " << cnt << " new length = " << length;
      CHECK_EQ(sizeof(length), ret);
      buffer = std::make_unique<Buffer>(length);
      buffer->set_msg_length(length);
    }

    ssize_t nbytes =
        sock_.Recv(buffer->GetRemainBuffer(), buffer->GetRemainSize());
    // DLOG(TRACE) << "recv data, cnt = " << cnt++ << " nbytes = " << nbytes;
    if (nbytes == -1) {
      CHECK(sock_.LastErrorWouldBlock()) << "errno = " << sock_.GetLastError();
      // EWOULDBLOCK, cannot receive anymore
      rx_buffers_.emplace_back(std::move(buffer));
      return;
    } else if (nbytes == 0) {
      LOG(WARNING) << "peer " << comm_id() << " has shutdown, disconnecting...";
      Disconnect();
      return;
    }

    buffer->MarkHandled(nbytes);
    if (!buffer->IsClear()) {
      // DLOG(TRACE) << "receive hasn't done, moving buffer to rx_buffers";
      rx_buffers_.emplace_back(std::move(buffer));
      break;
    } else {
      // got an message
      // DLOG(TRACE) << "got a message";
      HandleReceivedData(std::move(buffer));
      break;  /// handle one message once
    }
  }
}

void RdEndpoint::HandleReceivedData(std::unique_ptr<Buffer> buffer) {
  // dispatch according to type field
  SignalType type = GetTypeFromBuffer(buffer.get());
  // LOG(TRACE) << "type = " << kSignalTypeStr[static_cast<int>(type)];
  switch (type) {
    case SignalType::kUserData: {
      int src_comm_id = comm_id();
      LOG(TRACE) << "kUserData, src_comm_id: " << src_comm_id;
      comm_->meta_queue_.Push({src_comm_id, std::move(buffer)});
    } break;
    case SignalType::kFlowStart: {
      FlowStart* hdr = GetInHeader<FlowStart>(buffer.get());
      int msg_id = hdr->msg_id;
      // uint32_t flow_size = hdr->flow_size;
      uint32_t max_seq_num = hdr->max_seq_num;
      /// TODO(cjr): don't konw how to handle this max_seq_num
      msg_max_seq[msg_id] = max_seq_num;
      LOG(TRACE) << "kFlowStart, msg_id: " << msg_id
                 << " src_comm_id: " << comm_id()
                 << " max_seq_num: " << max_seq_num;

      /// TODO(cjr): finish this rendezvous later
      /// 1. rendezvous req

      /// 2. calculate rate adjustment

      /// 3. reply rate adjustment signal to all senders
    } break;
    case SignalType::kFlowFinish: {
      FlowFinish* hdr = GetInHeader<FlowFinish>(buffer.get());
      int msg_id = hdr->msg_id;
      LOG(TRACE) << "kFlowFinish, msg_id: " << msg_id << " src_comm_id: " << comm_id();

      CHECK(msg_max_seq.count(msg_id));

      /// 1. notify receiving channel
      ReceivingChannel::Notification n;
      n.type = ReceivingChannel::Notification::FINISH_FLOW;
      n.data.finish_flow = {msg_id, msg_max_seq[msg_id], conn_meta_};
      comm_->receiving_channel_->Notify(std::move(n));

    } break;
    case SignalType::kRateAdjustment: {
      RateAdjustment* hdr = GetInHeader<RateAdjustment>(buffer.get());
      LOG(TRACE) << "kRateAdjustment, src_comm_id: " << comm_id()
                 << ", rate: " << hdr->sending_rate;
      /// only this single writer
      double rate = conn_meta_->sending_rate.load();
      double throttle = std::max(static_cast<double>(hdr->sending_rate),
                                 MLTGlobal::Get()->InitialSendingRate());
      if (rate > throttle) {
        conn_meta_->sending_rate.store(throttle);
      } else {
        conn_meta_->sending_rate.store(rate * 2);
      }
    } break;
    case SignalType::kRetransmitRequest: {
      RetransmitRequest* hdr = GetInHeader<RetransmitRequest>(buffer.get());
      int msg_id = hdr->msg_id;
      int dest = hdr->comm_id;
      LOG(TRACE) << "kRetransmitRequest, msg_id: " << msg_id << ", src_comm_id: " << dest;

      /// notify priority channel of retransmit request
      PriorityChannel::Notification n;
      n.type = PriorityChannel::Notification::REQUEST_RETRANSMIT;
      n.req_buffer = std::move(buffer);
      comm_->priority_channel_->Notify(std::move(n));
    } break;
    case SignalType::kStopRequest: {
      StopRequest* hdr = GetInHeader<StopRequest>(buffer.get());
      int msg_id = hdr->msg_id;
      CHECK_EQ(hdr->comm_id, comm_id());
      LOG(TRACE) << "kStopRequest, msg_id: " << msg_id << " src_comm_id: " << comm_id();

      /// 1. notify priority channel of stopping this flow
      PriorityChannel::Notification n;
      n.type = PriorityChannel::Notification::STOP_FLOW;
      n.data.flow_id = EncodeFlow(comm_id(), msg_id);
      comm_->priority_channel_->Notify(std::move(n));

      /// 3. send this StopConfirm from priority channel thread instead of here
      /// see PriorityChannel::StopFlow()

      /// TODO(cjr): reconsider doing this here
      /// 4. submit to completion queue, also do this in packetize thread
      /// see PriorityChannel::StopFlow()
    } break;
    case SignalType::kStopConfirm: {
      StopConfirm* hdr = GetInHeader<StopConfirm>(buffer.get());
      int msg_id = hdr->msg_id;
      LOG(TRACE) << "kStopConfirm, msg_id: " << msg_id << " src_comm_id: " << comm_id();
      /// 1. notify receiving channel
      ReceivingChannel::Notification n;
      n.type = ReceivingChannel::Notification::CONFIRM_STOP;
      n.data.confirm_stop = {msg_id, conn_meta_};
      comm_->receiving_channel_->Notify(std::move(n));
  
      /// See ReceivingChannel::ConfirmStop()
      /// 1. maintain rate and flow information

      /// 4. release some resources, we can release here because it is never used in other places
      msg_max_seq.erase(msg_id);
    } break;
    default: {
      LOG(FATAL) << "unkown type: " << static_cast<uint32_t>(type);
    }
  }
}

void RdEndpoint::OnError() {
  // log something here
  set_dead(true);
}