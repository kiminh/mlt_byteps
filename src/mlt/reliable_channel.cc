#include "reliable_channel.h"
#include "mlt_communicator.h"
#include "conn_meta.h"

#include <queue>

ReliableChannel::~ReliableChannel() {
  if (!listening_sock_.IsClosed()) listening_sock_.Close();
}

void ReliableChannel::RemoveEndpoint(int dest) {
  auto it = ctrl_endpoints_.find(dest);
  if (it != ctrl_endpoints_.end() && it->second && !it->second->is_dead()) {
    it->second->Disconnect();
  }
}

void ReliableChannel::AddEndpoint(std::shared_ptr<RdEndpoint> endpoint) {
  epoll_helper_.EpollCtl(EPOLL_CTL_ADD, endpoint->fd(), &endpoint->event());

  int remote_comm_id = endpoint->comm_id();
  CHECK_EQ(0, ctrl_endpoints_.count(remote_comm_id));
  ctrl_endpoints_[remote_comm_id] = endpoint;

  endpoint->set_comm(comm_);
}

void ReliableChannel::HandleNewConnection() {
  TcpSocket new_sock = listening_sock_.Accept();
  new_sock.SetNonBlock(false);
  int rc_tos = prism::GetEnvOrDefault<int>("MLT_RC_TOS", 0xfe);
  int dest;
  CHECK_EQ(sizeof(dest), new_sock.Recv(&dest, sizeof(dest)));

  /// construct ConnMeta
  comm_->Lock();
  comm_->id_conn_[dest] = std::make_unique<ConnMeta>(dest);
  ConnMeta* conn_meta = comm_->id_conn_[dest].get();
  comm_->Unlock();

  // construct RdEndpoint
  auto new_endpoint =
      std::make_shared<RdEndpoint>(rc_tos, new_sock, conn_meta);
  new_endpoint->OnAccepted();
  AddEndpoint(new_endpoint);

  /// see also MLTCommunicator::AddConnection
  {
    PriorityChannel::Notification n;
    n.type = PriorityChannel::Notification::ADD_CONNECTION;
    n.data.conn = conn_meta;
    comm_->priority_channel_->Notify(std::move(n));
  }
}

void ReliableChannel::Notify(
    ReliableChannel::Notification&& notification) {
  notification_queue_.Push(std::move(notification));
}

void ReliableChannel::Enqueue(int dest, Buffer* buffer) {
  CHECK(0) << "should not go through this path";
  auto buffer_ptr = std::make_unique<Buffer>(buffer);
  tx_queue_.Push({dest, std::move(buffer_ptr)});
}

void ReliableChannel::Enqueue(int dest, std::unique_ptr<Buffer> buffer) {
  tx_queue_.Push({dest, std::move(buffer)});
}

void ReliableChannel::Listen(int port) {
  listen_port_ = port;

  AddrInfo ai(port, SOCK_STREAM);
  listening_sock_.Create(ai);
  listening_sock_.SetReuseAddr(true);
  listening_sock_.Bind(ai);
  listening_sock_.Listen();
  listening_sock_.SetNonBlock(true);

  listen_event_.events = EPOLLIN;
  listen_event_.data.fd = listening_sock_.sockfd;
  epoll_helper_.EpollCtl(EPOLL_CTL_ADD, listening_sock_.sockfd, &listen_event_);
}

void ReliableChannel::Run() {
  int timeout_ms = prism::GetEnvOrDefault<int>("EPOLL_TIMEOUT_MS", 1000);
  int max_events = prism::GetEnvOrDefault<int>("EPOLL_MAX_EVENTS", 1024);
  std::vector<struct epoll_event> events(max_events);

  std::queue<std::shared_ptr<RdEndpoint>> dead_eps;

  while (!terminated_.load()) {
    // Epoll IO
    int nevents = epoll_helper_.EpollWait(&events[0], max_events, timeout_ms);

    for (int i = 0; i < nevents; i++) {
      auto& ev = events[i];
      if (ev.data.fd == listening_sock_.sockfd) {
        CHECK(ev.events & EPOLLIN);
        HandleNewConnection();
        continue;
      }

      // data events
      RdEndpoint* endpoint = static_cast<RdEndpoint*>(ev.data.ptr);

      if (ev.events & EPOLLIN) {
        //printf("OnRecvReady\n");
        endpoint->OnRecvReady();
      }

      if (ev.events & EPOLLOUT) {
        //printf("OnSendReady\n");
        endpoint->OnSendReady();
      }

      if (ev.events & EPOLLERR) {
        LOG(WARNING) << "EPOLLERR, endpoint comm_id: " << endpoint->comm_id();
        endpoint->OnError();
        auto endpoint_ptr = std::move(ctrl_endpoints_[endpoint->comm_id()]);
        dead_eps.push(endpoint_ptr);
      }
    }

    // garbage collection first
    while (!dead_eps.empty()) {
      auto endpoint = std::move(dead_eps.front());
      /// In kernel versions before 2.6.9, the EPOLL_CTL_DEL operation required
      /// a non-null pointer in event, even though this argument is ignored.
      /// Since Linux 2.6.9, event can be specified as NULL  when  using
      /// EPOLL_CTL_DEL.  Applications that need to be portable to kernels
      /// before 2.6.9 should specify a non-null pointer in event.
      epoll_helper_.EpollCtl(EPOLL_CTL_DEL, endpoint->fd(), nullptr);
      dead_eps.pop();
    }

    ReliableChannel::Notification n;
    while (notification_queue_.TryPop(&n)) {
      switch (n.type) {
        case Notification::ADD_ENDPOINT: {
          AddEndpoint(n.endpoint);
        } break;
        case Notification::REMOVE_ENDPOINT: {
          RemoveEndpoint(n.data.dest_comm_id);
        } break;
        default: {
          LOG(FATAL) << "unknown notification type: "
                     << static_cast<int>(n.type);
        }
      }
    }

    // pull out packets and routing to corresponding endpoints
    std::tuple<int, std::unique_ptr<Buffer>> tup;
    while (tx_queue_.TryPop(&tup)) {
      int dest = std::get<0>(tup);
      auto buffer = std::move(std::get<1>(tup));
      auto it = ctrl_endpoints_.find(dest);
      
      /// ensure destination has been established and hasn't died
      if (it != ctrl_endpoints_.end() && it->second) {
        RdEndpoint* endpoint = it->second.get();
        endpoint->WriteLength(buffer.get());
        endpoint->tx_queue().push(std::move(buffer));
      }
    }
  }
}