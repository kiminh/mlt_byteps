#include "udp_endpoint.h"
#include "mlt_global.h"

UdpEndpoint::UdpEndpoint(int tos) {
  tos_ = tos;
  sock_.Create();
  sock_.SetNonBlock(true);
  sock_.SetTos(tos);

  event_.events = EPOLLOUT | EPOLLERR;
  event_.data.ptr = this;
}

UdpEndpoint::~UdpEndpoint() {
  if (!sock_.IsClosed()) sock_.Close();
}

// TODO(cjr): poll out all tx elements
ssize_t UdpEndpoint::OnSendReady() {
  ssize_t total_len = 0;
  // auto start = std::chrono::high_resolution_clock::now();
  while (!tx_queue_.empty()) {
    GradMessage pkt = tx_queue_.front();
    tx_queue_.pop();
    const SockAddr& addr = MLTGlobal::Get()->AddrFromCommId(pkt.dst_comm_id);
    // ssize_t nbytes = sock_.SendTo(pkt, pkt->len, 0, addr);

    // sendmsg
    struct msghdr msg;
    struct iovec iov[2];
    iov[0].iov_base = reinterpret_cast<void*>(&pkt);
    iov[0].iov_len = kGradPacketHeader;
    iov[1].iov_base = reinterpret_cast<void*>(pkt.grad_ptr);
    iov[1].iov_len = pkt.len - kGradPacketHeader;

    msg.msg_name = reinterpret_cast<void*>(const_cast<struct sockaddr*>(&addr.addr));
    msg.msg_namelen = addr.addrlen;
    msg.msg_iov = iov;
    msg.msg_iovlen = 2;
    msg.msg_control = nullptr;
    msg.msg_controllen = 0;

    ssize_t nbytes = sock_.SendMsg(&msg, 0);
    if (nbytes == -1) {
      CHECK(sock_.LastErrorWouldBlock()) << "errno = " << sock_.GetLastError();
      // cannot send anymore
      break;
    }
    CHECK_EQ(nbytes, pkt.len);

    total_len += nbytes;
  }
  // auto end = std::chrono::high_resolution_clock::now();
  // if (total_len > 0)
  // LOG(DEBUG) << prism::FormatString("total_len: %u, duration: %.3fus",
  //                                   total_len, (end - start).count() / 1e3);
  return total_len;
}