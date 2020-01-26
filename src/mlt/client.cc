#include <prism/logging.h>
#include "app_context.h"

#include "socket.h"
#include "packet.h"
#include "meter.h"
#include "epoll_helper.h"
#include "thread_proto.h"

const int kMaxPrio = 16;
const int kBasePort = 5555;

class ClientApp {
 public:
  ClientApp(AppContext& ctx) : ctx_{ctx} {}

  int Run(int base_tos);

  AppContext& ctx() { return ctx_; }

  ssize_t SendMsg(UdpSocket sock, const char* buf, size_t size, const SockAddr& addr, int base_tos);

 private:
  AppContext& ctx_;
};

ssize_t ClientApp::SendMsg(UdpSocket sock, const char* buf, size_t size, const SockAddr& addr, int base_tos) {
  struct msghdr msg;
  struct cmsghdr* cmsg;
  char cbuf[CMSG_SPACE(sizeof(int))];
  struct iovec iov[1];

  iov[0].iov_base = reinterpret_cast<void*>(const_cast<char*>(buf));
  iov[0].iov_len = size;

  msg.msg_name = reinterpret_cast<void*>(const_cast<struct sockaddr*>(&addr.addr));
  msg.msg_namelen = addr.addrlen;
  // LOG(INFO) << "namelen: " << addr.addrlen << " sizeof addr6: " << sizeof(addr.addr_storage);
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;
  msg.msg_control = cbuf;
  msg.msg_controllen = sizeof(cbuf);

  cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = IPPROTO_IP;
  cmsg->cmsg_type = IP_TOS;
  //cmsg->cmsg_level = IPPROTO_IPV6;
  //cmsg->cmsg_type = IPV6_TCLASS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));

  // int tos = ctx().conf().tos();
  int tos = rand() % 32 + base_tos;
  memcpy(CMSG_DATA(cmsg), &tos, sizeof(int));

  msg.msg_controllen = cmsg->cmsg_len;

  return sock.SendMsg(&msg, 0);
}

int ClientApp::Run(int base_tos) {
  std::string host = ctx().conf().host();
  AddrInfo ai(host.c_str(), kBasePort, SOCK_DGRAM, false);
  LOG(INFO) << "peer address: " << ai.AddrStr();

  EpollHelper epoller(0);
  const int max_events = 1024;
  const int timeout_ms = 1000;
  struct epoll_event events[max_events];

  // int num_conn = ctx().conf().num_conn();
  std::vector<UdpSocket> socks(kMaxPrio);
  std::vector<struct epoll_event> sock_event(kMaxPrio);

  // int tos = ctx().conf().tos();
  for (int i = 0; i < static_cast<int>(socks.size()); i++) {
    UdpSocket& sock = socks[i];
    sock.Create(ai);
    sock.SetNonBlock(true);
    // sock.SetNonBlock(false);
    sock.SetTos(base_tos + (i / 2) * 32 + i % 2);

    auto& sock_ev = sock_event[i];
    sock_ev.events = EPOLLOUT | EPOLLERR;
    sock_ev.data.ptr = &sock;
    epoller.EpollCtl(EPOLL_CTL_ADD, sock, &sock_event[i]);
  }

  size_t size = ctx().conf().message_size();
  char* buf = new char[size];
  strcpy(buf, "hello server\n");

  uint64_t num = 0;
  Packet *pkt = reinterpret_cast<Packet*>(buf);
  pkt->payload_len = size - sizeof(pkt);

  Meter meter(1000);

  int nrecv = ctx().conf().recv_threads();
  std::vector<SockAddr> addrs;
  for (int i = 0; i < nrecv; i++) {
    AddrInfo temp_ai(host.c_str(), kBasePort + i, false);
    addrs.emplace_back(temp_ai);
  }
  // SockAddr addr(ai);
  while (1) {
    // UdpSocket& sock = socks[num % kMaxPrio];
    // pkt->psn = num++;

    // SockAddr& addr = addrs[pkt->psn % nrecv];

    // //auto start = std::chrono::steady_clock::now();
    // ssize_t nbytes = sock.SendTo(buf, size, 0, addr);
    // //auto end = std::chrono::steady_clock::now();
    // //printf("send %ld bytes, cost %.3fus\n", nbytes, (end - start).count() / 1e3);
    // if (nbytes < 0) {
    //   printf("psn = %ld send failed, errno = %d\n", pkt->psn, sock.GetLastError());
    // } else {
    //   DLOG(INFO) << prism::FormatString("psn = %ld, %ld bytes sent", pkt->psn, nbytes);
    //   meter.Add(nbytes);
    // }

    int nevents = epoller.EpollWait(events, max_events, timeout_ms);

    for (int i = 0; i < nevents; i++) {
      auto& ev = events[i];
      if (ev.events & EPOLLIN) {
        continue;
      }

      if (ev.events & EPOLLOUT) {
        UdpSocket& sock = *static_cast<UdpSocket*>(ev.data.ptr);
        pkt->psn = num++;

        SockAddr& addr = addrs[pkt->psn % nrecv];

        // ssize_t nbytes = sock.SendTo(buf, size, 0, addr);
        ssize_t nbytes = SendMsg(sock, buf, size, addr, base_tos);
        if (nbytes < 0) {
          printf("psn = %ld send failed, errno = %d\n", pkt->psn, sock.GetLastError());
        } else {
          DLOG(INFO) << prism::FormatString("psn = %ld, %ld bytes sent", pkt->psn, nbytes);
          meter.Add(nbytes);
        }

      }

      if (ev.events & EPOLLERR) {
        LOG(INFO) << "EPOLLERR";
        continue;
      }
    }
  }

  for (UdpSocket& sock : socks) sock.Close();
  delete [] buf;
  return 0;
}

class SendingThread : public ThreadProto {
 public:
  SendingThread(ClientApp& app, int base_tos) : app_{app}, base_tos_{base_tos} {}
  virtual ~SendingThread() {}
  void Run() { app_.Run(base_tos_); }
 private:
  ClientApp& app_;
  int base_tos_;
};

int main(int argc, char *argv[]) {
  AppContext app_ctx;
  if (app_ctx.ParseArgument(argc, argv)) {
    app_ctx.ShowUsage(argv[0]);
    return 1;
  }
  ClientApp client_app(app_ctx);
  int send_threads = app_ctx.conf().send_threads();
  std::vector<std::unique_ptr<ThreadProto>> sending_ths(send_threads);
  int tos = app_ctx.conf().tos();
  for (auto& send_th : sending_ths) {
    send_th = std::make_unique<SendingThread>(client_app, tos);
    send_th->Start();
    tos += 32;
  }
  for (auto& send_th : sending_ths) {
    send_th->Join();
  }
  return 0;
}
