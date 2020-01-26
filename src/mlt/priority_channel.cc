#include "priority_channel.h"
#include "mlt_communicator.h"

void PriorityChannel::AddEndpoint(UdpEndpoint* endpoint) {
  int tos = endpoint->tos();
  CHECK(tos >= 0 && tos < kMaxPrio);
  CHECK(prio_mapping_[tos] == -1);

  epoll_helper_.EpollCtl(EPOLL_CTL_ADD, endpoint->fd(), &endpoint->event());
  endpoint->set_prio_channel(this);

  prio_mapping_[tos] = prio_endpoints_.size();
  prio_endpoints_.emplace_back(endpoint);
}

void PriorityChannel::Enqueue(int dest, const LtMessage& msg, PktPrioFunc* prio_func) {
  auto ltmsg_ext = std::make_unique<LtMessageExt>(msg);
  sr_queue_.Push({dest, std::move(ltmsg_ext), prio_func});
}

void PriorityChannel::Notify(
    PriorityChannel::Notification&& notification) {
  notification_queue_.Push(std::move(notification));
}

void PriorityChannel::Run() {
  int timeout_ms = prism::GetEnvOrDefault<int>("EPOLL_TIMEOUT_MS", 1000);
  int max_events = prism::GetEnvOrDefault<int>("EPOLL_MAX_EVENTS", 1024);
  std::vector<struct epoll_event> events(max_events);

  /// TODO(cjr): add statistics information in this loop

  meter_ = Meter(1000, "priority_channel", 0xff);

  while (!terminated_.load()) {
    // Epoll IO
    int nevents = epoll_helper_.EpollWait(&events[0], max_events, timeout_ms);

    for (int i = 0; i < nevents; i++) {
      auto& ev = events[i];
      UdpEndpoint* endpoint = static_cast<UdpEndpoint*>(ev.data.ptr);

      if (ev.events & EPOLLIN) {
        // endpoint->OnRecv();
      }

      if (ev.events & EPOLLOUT) {
        endpoint->OnSendReady();
      }

      if (ev.events & EPOLLERR) {
        LOG(WARNING) << "EPOLLERR, endpoint tos: " << endpoint->tos();
        // endpoint->OnError();
      }
    }

    /// TODO(cjr): change the polling frequency to optimize the performance
    /// poll send requests
    decltype(sr_queue_)::value_type sr;
    while (sr_queue_.TryPop(&sr)) {
      auto [dest, ltmsg_ext, prio_func] = std::move(sr);
      ConnMeta* conn_meta = FindConnMetaById(dest);
      int msg_id = ltmsg_ext->msg_id;
      CHECK(conn_meta->sending_msgs.count(msg_id) == 0)
          << "msg_id: " << msg_id << " is sending";
      conn_meta->sending_msgs[msg_id] = {std::move(ltmsg_ext), prio_func};
      DLOG(TRACE) << "pop a send request, dest: " << dest << " msg_id: " << msg_id;
    }
    /// round-robin, for each connection pull one packet from one message
    size_t total_len = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; i++) {
      total_len += PollSendingMessages();
      /// handle retransmitting requests
      total_len += PollRetransmitRequest();
    }
    auto end = std::chrono::high_resolution_clock::now();
    if (total_len > 0)
    DLOG(DEBUG) << prism::FormatString("total_len: %ld, duration: %.3fus",
                                      total_len, (end - start).count() / 1e3);
    /// handle notifications
    PollNotification();
  }

  // TODO(cjr): flush the packet_queue
}


size_t PriorityChannel::PollSendingMessages() {
  size_t bytes = 0;
  for (ConnMeta* conn_meta : conn_metas_) {
    GradPacket grad_packet;
    int dest = conn_meta->dest_comm_id;

    ConnMeta::SendRequest* send_request = nullptr;
    if (!conn_meta->sending_msgs.empty()) {
      send_request = &conn_meta->sending_msgs.begin()->second;
    }

    if (!send_request) continue;

    auto& tup = *send_request;
    LtMessageExt& ltmsg_ext = *std::get<0>(tup);
    PktPrioFunc* prio_func = std::get<1>(tup);

    /// sending rate throttle
    size_t nbytes = packetizer_->GetBytes(ltmsg_ext);
    auto& tx_meter = conn_meta->tx_meter;
    if (tx_meter.TryBytesPerSecond(nbytes) >
        conn_meta->sending_rate.load()) {
      continue;
    }

    packetizer_->PartitionOne(&grad_packet, dest, ltmsg_ext, prio_func);
    packetizer_->RoutePacket(grad_packet, grad_packet.is_last ? true : false);

    meter_.Add(grad_packet.len);
    bytes += grad_packet.len;

    /// update sending rate monitor
    if (tx_meter.Elapsed()) {
      tx_meter.Clear();
    }
    conn_meta->tx_meter.Update(nbytes);

    /// release this only when receiving kStopRequest
    if (ltmsg_ext.bytes_sent >= ltmsg_ext.size) {
      /// move to retransmitting_msgs
      auto msg_id = ltmsg_ext.msg_id;
      auto nh = std::move(conn_meta->sending_msgs.extract(msg_id));
      conn_meta->retransmitting_msgs.insert(std::move(nh));
    }
  }
  return bytes;
}

size_t PriorityChannel::PollRetransmitRequest() {
  size_t bytes = 0;
  for (ConnMeta* conn_meta : conn_metas_) {
    GradPacket grad_packet;
    int dest = conn_meta->dest_comm_id;

    ConnMeta::RetransmitState* state = nullptr;
    Buffer* buffer = nullptr;
    if (!conn_meta->retransmit_reqs.empty()) {
      auto& tup = conn_meta->retransmit_reqs.begin()->second;
      buffer = std::get<0>(tup).get();
      state = &std::get<1>(tup);
    }

    if (!buffer) continue;

    RetransmitRequest* hdr = GetInHeader<RetransmitRequest>(buffer);
    CHECK_EQ(dest, hdr->comm_id);
    auto it = conn_meta->retransmitting_msgs.find(hdr->msg_id);
    CHECK(it != conn_meta->retransmitting_msgs.end());
    LtMessageExt& ltmsg_ext = *std::get<0>(it->second);
    PktPrioFunc* prio_func = std::get<1>(it->second);

    /// sending rate throttle
    size_t nbytes = packetizer_->GetBytes(ltmsg_ext);
    auto& tx_meter = conn_meta->tx_meter;
    if (tx_meter.TryBytesPerSecond(nbytes) >
        conn_meta->sending_rate.load()) {
      continue;
    }

    CHECK_LT(state->block_num, hdr->num_blocks);
    Block& block = hdr->blocks[state->block_num];
    CHECK(block.first <= state->seq_num && state->seq_num < block.last)
        << "block.first: " << block.first
        << ", state->seq_num: " << state->seq_num
        << ", state->block_num: " << state->block_num
        << ", hdr->num_blocks:" << hdr->num_blocks
        << ", block.last: " << block.last;

    DLOG(TRACE) << "hdr->block.first: " << hdr->blocks[state->block_num].first
                << ", hdr->block.last: " << hdr->blocks[state->block_num].last
                << ", block_num: " << state->block_num
                << ", block_num: " << state->block_num
                << ", hdr->num_blocks: " << hdr->num_blocks;
    packetizer_->PartitionOneBySeq(&grad_packet, hdr->comm_id, ltmsg_ext,
                                   prio_func, state->seq_num);
    packetizer_->RoutePacket(grad_packet,
                             state->block_num + 1 == hdr->num_blocks &&
                                 state->seq_num + 1 == block.last);

    meter_.Add(grad_packet.len);
    bytes += grad_packet.len;

    /// update sending rate monitor
    if (tx_meter.Elapsed()) {
      tx_meter.Clear();
    }
    conn_meta->tx_meter.Update(nbytes);

    state->seq_num++;
    if (state->seq_num == block.last) {
      state->block_num++;
      if (state->block_num < hdr->num_blocks)
        state->seq_num = hdr->blocks[state->block_num].first;
    }

    if (state->block_num == hdr->num_blocks) {
      conn_meta->retransmit_reqs.erase(ltmsg_ext.msg_id);
    }
  }
  return bytes;
}

void PriorityChannel::PollNotification() {
  PriorityChannel::Notification n;
  while (notification_queue_.TryPop(&n)) {
    switch (n.type) {
      case Notification::ADD_CONNECTION: {
        ConnMeta* conn_meta = n.data.conn;
        conn_meta_map_[conn_meta->dest_comm_id] = conn_metas_.size();
        conn_metas_.emplace_back(conn_meta);
      } break;
      case Notification::REMOVE_CONNECTION: {
        int comm_id = n.data.conn->dest_comm_id;
        conn_meta_map_.erase(comm_id);
        conn_metas_.erase(
            std::find(conn_metas_.begin(), conn_metas_.end(), n.data.conn));
        /// this is a dirty handling way
        comm_->Lock();
        comm_->id_conn_[comm_id].reset();
        comm_->Unlock();
      } break;
      case Notification::STOP_FLOW: {
        FlowId flow_id = n.data.flow_id;
        StopFlow(flow_id);
      } break;
      case Notification::REQUEST_RETRANSMIT: {
        auto buffer = std::move(n.req_buffer);
        RetransmitRequest* hdr = GetInHeader<RetransmitRequest>(buffer.get());
        CHECK(hdr->type == SignalType::kRetransmitRequest)
            << "unexpected type: " << static_cast<int>(hdr->type);
        CHECK(hdr->num_blocks > 0);
        int comm_id = hdr->comm_id;
        int msg_id = hdr->msg_id;

        ConnMeta* conn_meta = FindConnMetaById(comm_id);
        auto value =
            std::make_tuple(std::move(buffer),
                            ConnMeta::RetransmitState(0, hdr->blocks[0].first));
        conn_meta->retransmit_reqs[msg_id] = std::move(value);
      } break;
      default: {
        LOG(FATAL) << "unknown notification type: "
                    << static_cast<int>(n.type);
      }
    }
  }
}


void PriorityChannel::StopFlow(FlowId flow_id) {
  auto [comm_id, msg_id] = DecodeFlow(flow_id);
  ConnMeta* conn_meta = FindConnMetaById(comm_id);

  // CHECK(conn_meta->sending_msgs.count(msg_id) == 0);
  auto it = conn_meta->sending_msgs.find(msg_id);
  if (it == conn_meta->sending_msgs.end()) {
    /// 1. stop the retransmitting requests
    if (conn_meta->retransmit_reqs.erase(msg_id) > 0) {
      DLOG(TRACE) << "comm_id: " << comm_id << " msg_id: " << msg_id
                  << " removed from retarnsmitting requests";
    }

    /// 2. release the holding retransmitting msgs records
    auto it2 = conn_meta->retransmitting_msgs.find(msg_id);
    if (it2 != conn_meta->retransmitting_msgs.end()) {
      conn_meta->retransmitting_msgs.erase(it2);
      DLOG(TRACE) << "comm_id: " << comm_id << " msg_id: " << msg_id
                  << " removed from retarnsmitting messages";
    }
  } else {
    conn_meta->sending_msgs.erase(it);
    DLOG(TRACE) << "comm_id: " << comm_id << " msg_id: " << msg_id
                << " removed from sending messages";
  }

  // CHECK(success) << "flow: " << flow_id << " already died";

  /// 3. send StopConfirm back
  auto new_buffer = std::make_unique<Buffer>(GetOutBufferSize<StopConfirm>());
  StopConfirm* new_hdr = GetOutHeader<StopConfirm>(new_buffer.get());
  new_hdr->type = SignalType::kStopConfirm;
  new_hdr->msg_id = msg_id;

  /// set buffer sending length and what length that receiver side sees
  new_buffer->set_msg_length(new_buffer->size());
  comm_->reliable_channel_->Enqueue(comm_id, std::move(new_buffer));

  /// 4. submit to completion queue, also do this in packetize thread
  Completion comp;
  comp.msg_id = msg_id;
  comp.type = CompletionType::kSend;
  comp.remote_comm_id = comm_id;
  // comp.bytes_sent = 0;
  comm_->cq_->Push(comp);
}

ConnMeta* PriorityChannel::FindConnMetaById(int comm_id) {
  // auto it = std::find_if(
  //     conn_metas_.begin(), conn_metas_.end(),
  //     [&comm_id](ConnMeta* c) { return c->dest_comm_id == comm_id; });
  // CHECK(it != conn_metas_.end()) << "cannot find comm_id: " << comm_id;
  // return *it;
  // return comm_->id_conn_[comm_id].get();  /// has concurrency issue
  return conn_metas_[conn_meta_map_[comm_id]];
}