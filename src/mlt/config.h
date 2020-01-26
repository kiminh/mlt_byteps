#include <getopt.h>
#include <stdio.h>
#include <string>

#define DEFAULT_IS_SERVER true
#define DEFAULT_MESSAGE_SIZE 1400
#define DEFAULT_BUFFER_SIZE 208 * 1024
#define DEFUALT_TOS 0
#define DEFUALT_NUM_CONN 1
#define DEFAULT_RECV_THREADS 1
#define DEFAULT_SEND_THREADS 1

class Config {
public:
  Config() {
    is_server_ = DEFAULT_IS_SERVER;
    message_size_ = DEFAULT_MESSAGE_SIZE;
    buffer_size_ = DEFAULT_BUFFER_SIZE;
    tos_ = DEFUALT_TOS;
    num_conn_ = DEFUALT_NUM_CONN;
    recv_threads_ = DEFAULT_RECV_THREADS;
    send_threads_ = DEFAULT_SEND_THREADS;
  }
  void set_is_server(bool is_server) { is_server_ = is_server; }
  bool is_server() const { return is_server_; }
  void set_host(const std::string &host) { host_ = host; }
  std::string host() const { return host_; }
  void set_buffer_size(size_t buffer_size) { buffer_size_ = buffer_size; }
  size_t buffer_size() const { return buffer_size_; }
  void set_message_size(size_t message_size) { message_size_ = message_size; }
  size_t message_size() const { return message_size_; }
  void set_tos(int tos) { tos_ = tos; }
  int tos() const { return tos_; }
  void set_num_conn(int num_conn) { num_conn_ = num_conn; }
  int num_conn() const { return num_conn_; }
  void set_recv_threads(int recv_threads) { recv_threads_ = recv_threads; }
  int recv_threads() const { return recv_threads_; }
  void set_send_threads(int send_threads) { send_threads_ = send_threads; }
  int send_threads() const { return send_threads_; }

private:
  bool is_server_;
  std::string host_;
  size_t message_size_;
  size_t buffer_size_;
  int tos_;
  int num_conn_;
  int recv_threads_;
  int send_threads_;
};

