#ifndef PS_MLT_VAN_H_
#define PS_MLT_VAN_H_
#define USE_MLT
#ifdef USE_MLT
namespace ps {
    /**
 * \file test_mlt_connection.cc
 * 
 * \brief this program establish connections according a host file
 * then it sends some meta data, and check the correctness of this procedure.
 */

#include "./mlt/mlt_communicator.h"
#include "./mlt/app_context.h"

#include "./mlt/string_helper.h"
#include "./mlt/meter.h"
#include "ps/internal/van.h"

#include <fstream>

#define GREEN_BOLD "\033[1;32m"
#define ESCAPE_END "\033[0m"

// Message.h:
// rank -> id       port -> port        host -> hostname
// rank, host_str, port
// struct Node {
//   int rank;
//   int port;
//   std::string host;
// };

enum class TestMode {
  CONNECTION,
  RC_CORRECTNESS,
  RC_SPEED,
  UDP_SIMPLE,
  UDP_SPEED,
};

static const char *g_test_mode_str[] = {
  "connection",
  "rc_correctness",
  "rc_speed",
  "udp_simple",
  "udp_speed"
};

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

class TestMLTApp : AppContext, public ZMQVan {
 public:
  TestMLTApp() {}
  virtual ~TestMLTApp() {}

  virtual int ParseArgument(int argc, char* argv[]) override;

  virtual void ShowUsage(const char* app) override;

  int Run();

  int RunTestConnection();

  int RunTestRcCorrectness();

  int RunTestRcSpeed();

  int RunTestUdpSimple();

  int RunTestUdpSpeed();
 
  void Barrier(int root);

  /// choose minimal rank as root rank
  inline int RootRank() {
    auto it = std::min_element(
        nodes_.begin(), nodes_.end(),
        [](const Node& a, const Node& b) { return a.id < b.id; });
    return it->id;
  }

 private:
  int Startup();

  int Finalize();

  // Van
  void Start(int customer_id);
  void Stop();
  void Connect(const Node &node);
  int Bind(const Node &node, int max_retry);
  int SendMsg(Message &msg);
  int RecvMsg(Message *msg);

  void ParseHostFile();

  Node ParseNode(const std::string& uri);

  inline Node FindNode(int rank) {
    auto it = std::find_if(nodes_.begin(), nodes_.end(),
                           [&rank](const Node& n) { return n.id == rank; });
    CHECK(it != nodes_.end()) << "could not find rank: " << rank;
    return *it;
  }

  inline std::string Hello(const Node& node) {
    return StringHelper::FormatString("Hello, %s:%d!", node.hostname.c_str(), node.port);
  }

  std::string host_file_;
  int my_rank_;
  TestMode mode_;

  /// we keep conf_ for compatibility but we do not use it

  std::unique_ptr<MLTCommunicator> mlt_comm_;
  std::vector<Node> nodes_;

  /// for rc_speed
  size_t meta_size_;
  /// for simple udp;
  size_t data_len_;
};

int TestMLTApp::ParseArgument(int argc, char *argv[]) {
  int err = 0;
  static struct option long_options[] = {
      {"help", no_argument, 0, 'h'},             // NOLINT(*)
      {"host-file", required_argument, 0, 'H'},  // NOLINT(*)
      {"rank", required_argument, 0, 'r'},       // NOLINT(*)
      {"mode", required_argument, 0, 'M'},       // NOLINT(*)
      {"meta-size", required_argument, 0, 'm'},  // NOLINT(*)
      {"data-len", required_argument, 0, 'l'},  // NOLINT(*)
      {0, 0, 0, 0}                               // NOLINT(*)
  };
  while (1) {
    int option_index = 0, c;
    c = getopt_long(argc, argv, "hH:r:M:m:l:", long_options, &option_index);
    if (c == -1) break;
    switch (c) {
      case 'h': {
        ShowUsage(argv[0]);
        exit(0);
      }
      case 'H': {
        const char* hostfile = optarg;
        host_file_ = std::string(hostfile);
        break;
      }
      case 'r': {
        int rank = atoi(optarg);
        my_rank_ = rank;
        break;
      }
      case 'M': {
        const std::string mode = std::string(optarg);
        auto mode_lower = StringHelper::ToLower(mode);
        bool found = false;
        for (size_t i = 0; i < ARRAY_SIZE(g_test_mode_str); i++) {
          if (!strcmp(mode_lower.c_str(), g_test_mode_str[i])) {
            found = true;
            mode_ = static_cast<TestMode>(i);
            break;
          }
        }
        if (!found) err = 2;
        break;
      }
      case 'm': {
        int meta_size = atoi(optarg);
        meta_size_ = meta_size;
        break;
      }
      case 'l': {
        int data_len = atoi(optarg);
        data_len_ = data_len;
        break;
      }
      case '?':
      default:
        err = 1;
        goto out;
    }
  }
  if (optind < argc) {
    err = 1;
    goto out;
  }
out:
  return err;
}

void TestMLTApp::ShowUsage(const char *app) {
  fprintf(stdout, "Usage:\n");
  fprintf(stdout, "  %s           start an agent and establish all-to-all connections\n", app);
  fprintf(stdout, "\nOptions:\n");
  fprintf(stdout, "  -h, --help              display this message\n");
  fprintf(stdout, "  -H, --host-file=<file>  host file\n");
  fprintf(stdout, "  -r, --rank=<int>        my rank\n");
  fprintf(stdout, "  -M, --mode=<int>        test mode ('connection', 'rc_correctness', 'rc_speed', 'udp_simple', 'udp_speed')\n");
  fprintf(stdout, "rc_speed options:\n");
  fprintf(stdout, "  -m, --meta-size=<int>   meta message size\n");
  fprintf(stdout, "udp_simple options:\n");
  fprintf(stdout, "  -l, --data-len=<int>    data length\n");
}

#define TRY(rc, func, ...)  \
  do {                      \
    rc = func(__VA_ARGS__); \
    if (rc) return rc;      \
  } while (0)

int TestMLTApp::Run() {
  int rc;
  TRY(rc, Startup);
  switch (mode_) {
    case TestMode::CONNECTION: {
      TRY(rc, RunTestConnection);
    } break;
    case TestMode::RC_CORRECTNESS: {
      TRY(rc, RunTestRcCorrectness);
    } break;
    case TestMode::RC_SPEED: {
      TRY(rc, RunTestRcSpeed);
    } break;
    case TestMode::UDP_SIMPLE: {
      TRY(rc, RunTestUdpSimple);
    } break;
    case TestMode::UDP_SPEED: {

    } break;
    default: {
      LOG(FATAL) << "unknown mode";
    }
  }
  TRY(rc, Finalize);
  return rc;
}

int TestMLTApp::Startup() {
  /// 1. parse host file
  ParseHostFile();

  Node my_node = FindNode(my_rank_);
  int my_listen_port = my_node.port; /// including udp and tcp port, and they are same

  /// 3. init mlt global with some parameters
  MLTGlobal::Get()->Init();

  /// 2. construct communicator and boot it
  mlt_comm_ = std::make_unique<MLTCommunicator>(my_rank_);
  MLTCommunicator* mlt_comm = mlt_comm_.get();
  mlt_comm->Start(my_listen_port);

  return 0;
}

int TestMLTApp::Finalize() {
  /// before remove connections, we must stop receiving first
  mlt_comm_->StopUdpReceiving();
  /// remove connections and finalize
  MLTCommunicator* mlt_comm = mlt_comm_.get();
  for (const Node& node : nodes_) {
    int rank = node.id;
    if (rank != my_rank_) mlt_comm->RemoveConnection(rank);
  }
  mlt_comm->Finalize();
  return 0;
}

void TestMLTApp::Start(int customer_id) override {
    should_stop_ = false;
    Van::Start(customer_id);
    my_rank_ = my_node_.id; // update my_rank_
    start_mu_.lock();
    // /// 1. parse host file
    // ParseHostFile();

    // Node my_node = FindNode(my_rank_);
    // int my_listen_port = my_node.port; /// including udp and tcp port, and they are same

    /// 3. init mlt global with some parameters
    MLTGlobal::Get()->Init();

    start_mu_.unlock();
    return;
}

void TestMLTApp::Stop() override {
    PS_VLOG(1) << my_node_.ShortDebugString() << " is stopping";
    Van::Stop();

    should_stop_ = true;
    mlt_comm_->StopUdpReceiving();

    MLTCommunicator* mlt_comm = mlt_comm_.get();
    for (const Node& node : nodes_) {
        int rank = node.id;
        if (rank != my_rank_) mlt_comm->RemoveConnection(rank);
    }
    mlt_comm->Finalize();
    return;
}

int Bind(const Node &node, int max_retry) override {
    // Node my_node = FindNode(my_rank_);
    MLTCommunicator* mlt_comm = mlt_comm_.get();

    mlt_comm_ = std::make_unique<MLTCommunicator>(my_rank_);
    MLTCommunicator* mlt_comm = mlt_comm_.get();
    mlt_comm->Start(my_node_.port);
    return 0;
}

void Connect(const Node &node) override {
    // Node my_node = FindNode(my_rank_);
    MLTCommunicator* mlt_comm = mlt_comm_.get();

    int rank = node.id;
    if (rank == my_rank_) continue;
    mlt_comm->AddConnection(rank, node.hostname, node.port);
    nodes_.push_back(node); // Copy connection list in mlt_val
}

int SendMsg(Message &msg) override {
    if (!is_worker_) ; // if is not worker
    /// 5. say "Hello, host:port!" and check
    // Node my_node = FindNode(my_rank_);
    MLTCommunicator* mlt_comm = mlt_comm_.get();

    // First send metadata
    std::lock_guard<std::mutex> lk(mu_);

    int rank = msg.meta.recver;
    CHECK_NE(rank, Meta::kEmpty);

    return PktSendMsg(rank, msg);
}

int PktSendMsg(int rank, Message &msg) {
    MLTCommunicator* mlt_comm = mlt_comm_.get();

    // send meta
    int meta_size;
    char* meta_buf = nullptr;
    PackMeta(msg.meta, &meta_buf, &meta_size);
    mlt_comm->SendMetaAsync(rank, meta_buf);

    // send data
    int send_bytes = meta_size;
    int n = msg.data.size();
    for (int i = 0; i < n; ++i) {
        SArray<char>* data = new SArray<char>(msg.data[i]);
        int data_size = data->size();
        
    }
    // Next send data
    for (const Node& node : nodes_) {
        int rank = node.id;
        if (rank == my_rank_) continue;

        // Send data
        int n = msg.data.size();
        for (int i = 0; i < n; i++) {
            mlt_comm->SendMetaAsync(rank, msg.data->data())
        }
    }
}

int RecvMsg(Message *msg) override {
    Node my_node = FindNode(my_rank_);
    MLTCommunicator* mlt_comm = mlt_comm_.get();

    for (size_t i = 0; i + 1 < nodes_.size(); i++) {
        
    }
}

int TestMLTApp::RunTestConnection() {
  /// 5. say "Hello, host:port!" and check
  Node my_node = FindNode(my_rank_);
  MLTCommunicator* mlt_comm = mlt_comm_.get();
  for (const Node& node : nodes_) {
    int rank = node.rank;
    if (rank == my_rank_) continue;

    /// prepare buffer
    std::string greeting = Hello(node);
    Buffer send_buffer(greeting.c_str(), greeting.size(), greeting.size());

    /// send it
    mlt_comm->SendMetaAsync(rank, &send_buffer);
  }

  /// 5.2 receive meta and check
  std::string expected_greeting = Hello(my_node);

  for (size_t i = 0; i + 1 < nodes_.size(); i++) {
    int rank;
    std::string recv_string(expected_greeting.size(), '0');
    Buffer recv_buffer(recv_string.c_str(), recv_string.size(),
                       recv_string.size());

    mlt_comm->RecvMeta(&rank, &recv_buffer);

    /// check it
    auto it = std::find_if(nodes_.begin(), nodes_.end(),
                           [&rank](auto& n) { return rank == n.rank; });
    CHECK(it != nodes_.end()) << "receive from unknown rank: " << rank;

    CHECK_EQ(recv_buffer.msg_length(),
             static_cast<uint32_t>(expected_greeting.size()));
    CHECK_EQ(recv_string, expected_greeting);
  }

  LOG(INFO) << GREEN_BOLD << "pass connection test!" << ESCAPE_END;
  return 0;
}

int TestMLTApp::RunTestRcCorrectness() {
  LOG(FATAL) << "implement this";
  return 1;
}

int TestMLTApp::RunTestRcSpeed() {
  Node my_node = FindNode(my_rank_);
  MLTCommunicator* mlt_comm = mlt_comm_.get();

  CHECK_GT(meta_size_, 0);
  char* buf = new char[meta_size_];
  memset(buf, 'a', meta_size_);

  Meter meter(1000, "test_rc_speed");
  int max_rank = -1;
  for (const Node& node : nodes_)
    if (max_rank < node.rank) max_rank = node.rank;

  std::vector<size_t> send_cnt(max_rank + 1);
  std::vector<size_t> recv_cnt(max_rank + 1);

  const size_t max_gap = 320;  /// max_gap > 0
  while (1) {
    /// 1. send meta
    for (const Node& node : nodes_) {
      int rank = node.rank;
      if (rank == my_rank_) continue;

      /// send it
      if (send_cnt[rank] <= recv_cnt[rank] + max_gap) {
        /// prepare buffer
        Buffer send_buffer(buf, meta_size_, meta_size_);
        mlt_comm->SendMetaAsync(rank, &send_buffer);
        // printf("Send a message, cnt = %ld\n", send_cnt[rank]); fflush(stdout);
        send_cnt[rank]++;
      } else {
        int n_rank;
        Buffer recv_buffer(buf, meta_size_, meta_size_);

        mlt_comm->RecvMeta(&n_rank, &recv_buffer);
        // printf("Recv a message, cnt = %ld\n", recv_cnt[n_rank]); fflush(stdout);
        //LOG(INFO) << "n_rank: " << n_rank;

        /// check it
        auto it = std::find_if(nodes_.begin(), nodes_.end(),
                              [&n_rank](auto& n) { return n_rank == n.rank; });
        CHECK(it != nodes_.end()) << "receive from unknown rank: " << n_rank;

        CHECK_EQ(recv_buffer.msg_length(), meta_size_);
        recv_cnt[n_rank]++;
        meter.Add(meta_size_);
      }
    }
    // auto start = std::chrono::high_resolution_clock::now();

    // auto end = std::chrono::high_resolution_clock::now();
    // printf("%.3fus\n", (end - start).count() / 1e3);
  }

  delete [] buf;
  LOG(INFO) << GREEN_BOLD << "pass rc_speed test!" << ESCAPE_END;
  return 0;
}

int TestMLTApp::RunTestUdpSimple() {
  Node my_node = FindNode(my_rank_);
  MLTCommunicator* mlt_comm = mlt_comm_.get();

  /// 1. create completion queue and bind it to communicator
  auto cq = std::make_unique<CompletionQueue>();
  mlt_comm->SetCompletionQueue(cq.get());

  RandomGenerator gen;
  float* gradients = new float[data_len_];
  // for (size_t i = 0; i < data_len_; i++)
  //   gradients[i] = gen.Uniform(-0.1, 0.1);
  LtMessage ltmsg;
  ltmsg.buf = reinterpret_cast<char*>(gradients);
  ltmsg.size = sizeof(float) * data_len_;
  ltmsg.msg_id = 5;

  float* recv_gradients = new float[data_len_];
  LtMessage recv_ltmsg;
  recv_ltmsg.buf = reinterpret_cast<char*>(recv_gradients);
  recv_ltmsg.size = sizeof(float) * data_len_;
  recv_ltmsg.msg_id = 5;

  PktPrioFunc* prio_func = new DefaultPktPrioFunc("layer_1", 1, 0.5);

  auto start = std::chrono::high_resolution_clock::now();

  /// send to other nodes and post recv at the same time
  for (const Node& node : nodes_) {
    int rank = node.rank;
    if (rank == my_rank_) continue;

    mlt_comm->PostRecv(rank, recv_ltmsg, 0.1);
  }

  /// barrier here
  Barrier(RootRank());
  Barrier(RootRank());

  /// send to other nodes and post recv at the same time
  for (const Node& node : nodes_) {
    int rank = node.rank;
    if (rank == my_rank_) continue;

    mlt_comm->PostSend(rank, ltmsg, prio_func);
  }

  /// poll completions
  int num_comps = 0;
  const int max_comps = 32;
  Completion comps[max_comps];
  while (num_comps + 2 < 2 * static_cast<int>(nodes_.size())) {
    int ret = cq->PollOnce(max_comps, comps);
    num_comps += ret;
    for (int i = 0; i < ret; i++) {
      if (comps[i].type == CompletionType::kSend) {
        LOG(INFO) << "send completion to dest " << comps[i].remote_comm_id;
        CHECK_EQ(comps[i].msg_id, 5);
      } else if (comps[i].type == CompletionType::kRecv) {
        LOG(INFO) << "recv completion, " << comps[i].bytes_received
                  << " bytes received from dest " << comps[i].remote_comm_id;
        CHECK_EQ(comps[i].msg_id, 5);
      } else {
        LOG(FATAL) << "unknown completion type: " << static_cast<int>(comps[i].type);
      }
    }
  }

  // mlt_comm->StopUdpReceiving();

  /// barrier
  Barrier(RootRank());
  /// RunTestConnection(); // as an barrier

  auto end = std::chrono::high_resolution_clock::now();

  LOG(INFO) << GREEN_BOLD << "pass udp_simple test!" << ESCAPE_END
            << prism::FormatString(" time elapsed: %.3fms",
                                   (end - start).count() / 1e6);

  delete prio_func;
  delete [] recv_gradients;
  delete [] gradients;
  return 0;
}

void TestMLTApp::Barrier(int root) {
  LOG(DEBUG) << "Barrier, root: " << root;
  auto s = std::chrono::high_resolution_clock::now();

  auto send_buffer = std::make_unique<Buffer>(sizeof(int));
  send_buffer->set_msg_length(sizeof(int));
  *reinterpret_cast<int*>(send_buffer->ptr()) = my_rank_;

  auto recv_buffer = std::make_unique<Buffer>(sizeof(int));

  if (my_rank_ == root) {
    std::vector<int> ranks;
    for (const Node& n: nodes_) {
      if (n.rank != root) {
        mlt_comm_->SendMetaAsync(n.rank, send_buffer.get());
      }
    }
    for (const Node& n: nodes_) {
      if (n.rank != root) {
        int dest;
        mlt_comm_->RecvMeta(&dest, recv_buffer.get());
        FindNode(dest);
        ranks.push_back(dest);
      }
    }
    std::sort(ranks.begin(), ranks.end());
    auto last = std::unique(ranks.begin(), ranks.end());
    CHECK(last == ranks.end());
  } else {
    int dest = -1;
    mlt_comm_->RecvMeta(&dest, recv_buffer.get());
    CHECK_EQ(dest, root);
    mlt_comm_->SendMetaAsync(root, send_buffer.get());
  }

  auto e = std::chrono::high_resolution_clock::now();
  LOG(INFO) << prism::FormatString("Barrier time: %.3fus\n", (e - s).count() / 1e3);
}

Node TestMLTApp::ParseNode(
    const std::string& uri) {
  auto pos0 = uri.find_last_of(',');
  CHECK_NE(pos0, std::string::npos);
  std::string rank_str = uri.substr(0, pos0);
  int rank = std::stoi(rank_str, &pos0);
  CHECK_EQ(pos0, rank_str.length());

  auto pos = uri.find_last_of(':');
  CHECK_NE(pos, std::string::npos);

  std::string host_str = uri.substr(pos0 + 1, pos - pos0 - 1);
  std::string port_str = uri.substr(pos + 1);

  int port = std::stoi(port_str, &pos);
  CHECK_EQ(pos, port_str.length());

  return {rank, port, host_str};
}


void TestMLTApp::ParseHostFile() {
  std::fstream fs(host_file_, std::ios::in);
  CHECK(fs.is_open()) << "failed to open host file: " << host_file_;

  for (std::string line; std::getline(fs, line); ) {
    StringHelper::Trim(line, " \n\r\t", line);
    if (line.empty()) continue;
    if (line[0] == '#') continue;

    nodes_.emplace_back(ParseNode(line));
  }
}


// int main(int argc, char* argv[]) {
//   TestMLTApp app;
//   if (int err; (err = app.ParseArgument(argc, argv))) {
//     app.ShowUsage(argv[0]);
//     return err;
//   }
//   return app.Run();
// }
}
#endif // USE_MLT
#endif// PS_MLT_VAN_H_