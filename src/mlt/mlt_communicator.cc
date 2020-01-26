#include "mlt_communicator.h"

void MLTCommunicator::Start(int listen_port) {
  int num_priorities = prism::GetEnvOrDefault<int>("MLT_NUM_PRIO", 8);
  if (num_priorities > MLTGlobal::Get()->NumQueues())
    num_priorities = MLTGlobal::Get()->NumQueues();

  int wq_size = prism::GetEnvOrDefault<int>("MLT_WQ_SIZE", 32);
  priority_channel_ = std::make_unique<PriorityChannel>(this, wq_size);
  for (int i = 0; i < num_priorities; i++) {
    int dscp = i * 8;
    int ect = 1, non_ect = 0;

    for (int ecn : {ect, non_ect}) {
      UdpEndpoint* endpoint = new UdpEndpoint((dscp << 2) | ecn);
      priority_channel_->AddEndpoint(endpoint);
    }
  }
  priority_channel_->Start();

  // int task_queue_size = prism::GetEnvOrDefault<int>("MLT_TASK_QUEUE_SIZE", 32);
  // packetizer_ = std::make_unique<Packetizer>(this, task_queue_size);
  // packetizer_->Start();

  int udp_port = prism::GetEnvOrDefault<int>("MLT_UDP_PORT", 5555);
  /// overwrite with command line argument
  if (listen_port) udp_port = listen_port;
  receiving_channel_ = std::make_unique<ReceivingChannel>(this, udp_port, wq_size);
  receiving_channel_->Start();

  int rc_queue_size = prism::GetEnvOrDefault<int>("MLT_RC_QUEUE_SIZE", 32);
  reliable_channel_ = std::make_unique<ReliableChannel>(this, rc_queue_size);
  reliable_channel_->Start();

  int rc_port = prism::GetEnvOrDefault<int>("MLT_RC_PORT", 5555);
  /// overwrite with command line argument
  if (listen_port) rc_port = listen_port;
  reliable_channel_->Listen(rc_port);
}

void MLTCommunicator::Finalize() {
  priority_channel_->Terminate();
  receiving_channel_->Terminate();
  reliable_channel_->Terminate();

  priority_channel_->Join();
  receiving_channel_->Join();
  reliable_channel_->Join();
}

void MLTCommunicator::StopUdpReceiving() {
  receiving_channel_->Terminate();
}

void MLTCommunicator::AddConnection(int dest_comm_id, const std::string& host, int port) {
  // std::lock_guard<std::mutex> lk(mu_);
  // MLTGlobal collection mapping
  AddrInfo ai(host.c_str(), port, SOCK_STREAM);
  SockAddr addr(ai);
  MLTGlobal::Get()->AddCommIdAddr(dest_comm_id, addr);

  // we only do the real connecting when this_comm_id < other_comm_id
  if (comm_id_ >= dest_comm_id) {
    /// wait for connection being accepted
    bool connected = false;
    while (!connected) {
      Lock();
      if (!id_conn_.count(dest_comm_id) == 0) connected = true;
      Unlock();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return;
  }

  // construct conn_meta
  ConnMeta* conn_meta = nullptr;
  Lock();
  if (id_conn_.count(dest_comm_id) == 0) {
    id_conn_[dest_comm_id] = std::make_unique<ConnMeta>(dest_comm_id);
  }
  conn_meta = id_conn_[dest_comm_id].get();
  Unlock();

  // establish rd endpoint and connect and set non blocking
  int rc_tos = prism::GetEnvOrDefault<int>("MLT_RC_TOS", 0xfe);

  auto endpoint = std::make_shared<RdEndpoint>(rc_tos, conn_meta);
  // note: this call may block
  endpoint->Connect();
  CHECK_EQ(sizeof(comm_id_), endpoint->sock().Send(&comm_id_, sizeof(comm_id_)));
  endpoint->OnConnected();

  /// send notication to add endpoint
  {
    ReliableChannel::Notification n;
    n.type = ReliableChannel::Notification::ADD_ENDPOINT;
    n.endpoint = endpoint;
    reliable_channel_->Notify(std::move(n));
    // reliable_channel_->AddEndpoint(endpoint);
  }

  /// send notification to add connection, see also
  /// ReliableChannel::HandleNewConnection
  {
    PriorityChannel::Notification n;
    n.type = PriorityChannel::Notification::ADD_CONNECTION;
    n.data.conn = conn_meta;
    priority_channel_->Notify(std::move(n));
  }
}

void MLTCommunicator::RemoveConnection(int dest_comm_id) {
  {
    Lock();
    ConnMeta* conn_meta = id_conn_[dest_comm_id].get();
    Unlock();

    PriorityChannel::Notification n;
    n.type = PriorityChannel::Notification::REMOVE_CONNECTION;
    n.data.conn = conn_meta;
    priority_channel_->Notify(std::move(n));
  }

  {
    ReliableChannel::Notification n;
    n.type = ReliableChannel::Notification::REMOVE_ENDPOINT;
    n.data.dest_comm_id = dest_comm_id;
    reliable_channel_->Notify(std::move(n));
  }
  // reliable_channel_->RemoveEndpoint(dest_comm_id);
  /// this will cause concurrency problem

  /// let this handle by priority_channel, see
  /// PriorityChannel::PollNotification()
  /// id_conn_[dest_comm_id].reset();
}

/// because meta data are usually small messages, so we use copy here
void MLTCommunicator::SendMetaAsync(int dest, Buffer* buffer) {
  auto new_buffer = std::make_unique<Buffer>(
      buffer->msg_length() + GetOutBufferSize<UserDataHeader>());

  new_buffer->set_msg_length(new_buffer->size());
  /// fill header field
  UserDataHeader* hdr = GetOutHeader<UserDataHeader>(new_buffer.get());
  hdr->type = SignalType::kUserData;
  /// copy data
  memcpy(hdr->payload, buffer->ptr(), buffer->msg_length());

  reliable_channel_->Enqueue(dest, std::move(new_buffer));
}

void MLTCommunicator::RecvMeta(int* dest, Buffer* buffer) {
  std::tuple<int, std::unique_ptr<Buffer>> tup;
  meta_queue_.WaitAndPop(&tup);
  *dest = std::get<0>(tup);
  auto new_buffer = std::move(std::get<1>(tup));
  UserDataHeader* hdr = GetInHeader<UserDataHeader>(new_buffer.get());
  buffer->set_msg_length(new_buffer->msg_length() - sizeof(*hdr));
  memcpy(buffer->ptr(), hdr->payload, buffer->msg_length());
  /// TODO(cjr): optimize this, remove alloc/free, remove copy
  /// return or delete buffer
  // delete new_buffer;
}

void MLTCommunicator::PostSend(int dest, const LtMessage& msg,
                               PktPrioFunc* prio_func) {
  // CHECK_NOTNULL(id_conn_[dest])->
  /// 1. flow start notification
  auto buffer = std::make_unique<Buffer>(GetOutBufferSize<FlowStart>());
  FlowStart* hdr = GetOutHeader<FlowStart>(buffer.get());
  hdr->type = SignalType::kFlowStart;
  hdr->msg_id = msg.msg_id;
  hdr->flow_size = msg.size;
  hdr->max_seq_num = priority_channel_->packetizer()->GetMaxSeqNum(msg.size);
  buffer->set_msg_length(buffer->size());
  reliable_channel_->Enqueue(dest, std::move(buffer));

  /// 2. give message to packetizer
  priority_channel_->Enqueue(dest, msg, prio_func);

  /// 3. flow finish notification
}

void MLTCommunicator::PostRecv(int dest, const LtMessage& msg,
                               double loss_ratio) {
  /// Attention: concurrently access of id_conn_
  Lock();
  ConnMeta* conn_meta = CHECK_NOTNULL(id_conn_[dest]).get();
  Unlock();

  receiving_channel_->Enqueue(conn_meta, msg, loss_ratio);
}