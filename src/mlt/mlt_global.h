#ifndef MLT_GLOBAL_H_
#define MLT_GLOBAL_H_

#include "socket.h"
#include "grad_packet.h"


struct MLTGlobal {
  static MLTGlobal* Get() {
    static MLTGlobal inst;
    return &inst;
  }

  void Init();

  const std::string& ModelName() const { return model_name; };
  int LayerId();
  double Theta();
  int NumPriorityChannels();
  int MaxPrio();
  void AddCommIdAddr(int comm_id, const SockAddr& addr);
  const SockAddr& AddrFromCommId(int comm_id) const;
  int Mtu();
  int MaxSegment();
  int NumLayers() const { return num_layers; }
  int NumQueues();
  int Bdp();
  int InitialSendingWindow();
  double InitialSendingRate();
  uint64_t RateMonitorIntervalUs();
  size_t ConnectionBacklogSize();

  std::vector<SockAddr> id_addr;

  std::string model_name;
  int mtu;
  int num_queues;
  int num_layers;
  // bandwidth delay product
  int bdp;
  int init_send_window;
  double init_send_rate;
  uint64_t rate_monitor_interval_us;
  size_t conn_backlog_size;

 private:
  MLTGlobal() {}
  ~MLTGlobal() {}
};

#endif  // MLT_GLOBAL_H_