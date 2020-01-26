#include "app_context.h"
#include "socket.h"
#include "packet.h"
#include "meter.h"
#include "utils.h"
#include "thread_proto.h"

class ServerApp {
 public:
  ServerApp(AppContext& ctx) : ctx_{ctx} {}

  int Run(int port);
  
  void SetSockBuffer(UdpSocket sock);

  AppContext& ctx() { return ctx_; }

  ssize_t RecvMsg(UdpSocket sock, const char* buf, size_t size, SockAddr* addr);

 private:
  AppContext& ctx_;
};

void Print(ssize_t nbytes, char* buf) {
  printf("nbytes = %ld\n", nbytes);
  for (ssize_t i = 0; i < nbytes; i++) {
    putchar(buf[i]);
  }
}

ssize_t ServerApp::RecvMsg(UdpSocket sock, const char* buf, size_t size, SockAddr* addr) {
  ssize_t ret;
  struct msghdr msg;
  struct cmsghdr* cmsg;
  char cbuf[CMSG_SPACE(sizeof(int))];
  struct iovec iov[1];

  iov[0].iov_base = reinterpret_cast<void*>(const_cast<char*>(buf));
  iov[0].iov_len = size;

  msg.msg_name = reinterpret_cast<void*>(addr);
  msg.msg_namelen = sizeof(addr->addr_storage);
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cbuf;
  msg.msg_controllen = sizeof(cbuf);

  int tos;
  struct in_pktinfo in_pktinfo;
  struct in6_pktinfo in6_pktinfo;

  ret = sock.RecvMsg(&msg, 0);
  if (ret >= 0) {
    cmsg = CMSG_FIRSTHDR(&msg);
    while (cmsg) {
      if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_TOS) {
        tos = *reinterpret_cast<int*>(CMSG_DATA(cmsg));
        LOG(INFO) << "recv tos: " << tos;
      }
      if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
        memcpy(&in_pktinfo, CMSG_DATA(cmsg), sizeof(in_pktinfo));
        LOG(INFO) << "have in_pktinfo";
      }
      if (cmsg->cmsg_level == IPPROTO_IPV6 && cmsg->cmsg_type == IPV6_PKTINFO) {
        memcpy(&in6_pktinfo, CMSG_DATA(cmsg), sizeof(in6_pktinfo));
        LOG(INFO) << "have in6_pktinfo";
      }
      cmsg = CMSG_NXTHDR(&msg, cmsg);
    }
  }
  return ret;
}

/// sysctl net.core.rmem_max=104857600
void ServerApp::SetSockBuffer(UdpSocket sock) {
  size_t buffer_size = ctx().conf().buffer_size();
  sock.SetRecvBuffer(buffer_size);
  int real_size;
  sock.GetRecvBuffer(&real_size);
  LOG(INFO) << prism::FormatString(
      "socket receive buffer size: %s (requested: %s)",
      FormatBytes(real_size, "KB").c_str(), FormatBytes(buffer_size, "KB").c_str());
}

int ServerApp::Run(int port) {
  AddrInfo ai(port, SOCK_DGRAM);

  UdpSocket sock;
  sock.Create(ai);
  sock.SetReuseAddr(true);
  sock.SetNonBlock(false);

  sock.Bind(ai);
  LOG(INFO) << "Bind at address: " << ai.AddrStr();

  SetSockBuffer(sock);

  const int on = 1, off = 0;
  PCHECK(!setsockopt(sock, IPPROTO_IP, IP_PKTINFO, &on, sizeof(on)));
  //PCHECK(!setsockopt(sock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on, sizeof(on)));
  //PCHECK(!setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)));
  (void)off;

  size_t size = ctx().conf().message_size();
  char* buf = new char[size];
  int64_t last_psn = -1;

  Meter meter(1000);

  while (1) {
    SockAddr client_addr;
    ssize_t nbytes = sock.RecvFrom(buf, size, 0, &client_addr);

    //ssize_t nbytes = RecvMsg(sock, buf, size, &client_addr);

    if (nbytes == -1) {
      if (!sock.LastErrorWouldBlock()) {
        PLOG(INFO) << "errno = " << sock.GetLastError();
      } else {
        printf("WouldBlock\n");
      }
    } else {
      meter.Add(nbytes);
      Packet* pkt = reinterpret_cast<Packet*>(buf);
      // printf("psn = %ld\n", pkt->psn);
      if (static_cast<int64_t>(pkt->psn) != last_psn + 1) {
        DLOG(INFO) << prism::FormatString("last = %ld, this = %ld", last_psn, pkt->psn);
      }
      last_psn = pkt->psn;
    }
    // Print(nbytes, buf);
  }

  delete [] buf;
}

class ReceivingThread : public ThreadProto {
 public:
  ReceivingThread(ServerApp& app, int port) : app_{app}, port_{port} {}
  virtual ~ReceivingThread() {}
  void Run() { app_.Run(port_); }
 private:
  ServerApp& app_;
  int port_;
};

int main(int argc, char* argv[]) {
  AppContext app_ctx;
  if (app_ctx.ParseArgument(argc, argv)) {
    app_ctx.ShowUsage(argv[0]);
    return 1;
  }
  ServerApp server_app(app_ctx);
  int recv_threads = app_ctx.conf().recv_threads();
  std::vector<std::unique_ptr<ThreadProto>> receiving_ths(recv_threads);
  int port = 5555;
  for (auto& recv_th : receiving_ths) {
    recv_th = std::make_unique<ReceivingThread>(server_app, port);
    recv_th->Start();
    port++;
  }
  for (auto& recv_th : receiving_ths) {
    recv_th->Join();
  }
  return 0;
}
