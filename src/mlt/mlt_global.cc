#include "mlt_global.h"

void MLTGlobal::Init() {
  /// initialize these variables
  /// 1. get model name
  model_name = "resnet50";
  /// 2. get number of layers
  num_layers = 228;
}

void MLTGlobal::AddCommIdAddr(int comm_id, const SockAddr& addr) {
  if (comm_id >= static_cast<int>(id_addr.size())) id_addr.resize(comm_id + 1);
  id_addr[comm_id] = addr;
}

const SockAddr& MLTGlobal::AddrFromCommId(int comm_id) const {
  return id_addr.at(comm_id);
}

int MLTGlobal::Mtu() {
  if (mtu == 0) {
    mtu = prism::GetEnvOrDefault<int>("MLT_MTU", 1500);
  }
  return mtu;
}

int MLTGlobal::MaxSegment() {
  return Mtu() - 28;
}

int MLTGlobal::NumQueues() {
  if (num_queues == 0) {
    num_queues = prism::GetEnvOrDefault<int>("MLT_NUM_QUEUES", 8);
  }
  return num_queues;
}

int MLTGlobal::Bdp() {
  if (bdp == 0) {
    bdp = prism::GetEnvOrDefault<int>("MLT_BDP", 400 * 1024);
  }
  return bdp;
}

int MLTGlobal::InitialSendingWindow() {
  if (init_send_window == 0) {
    init_send_window = prism::GetEnvOrDefault<int>("MLT_INIT_WINDOW", 0);
    if (init_send_window == 0) init_send_window = Bdp() * 1e4;
  }
  return init_send_window;
}

double MLTGlobal::InitialSendingRate() {
  if (init_send_rate == 0) {
    init_send_rate = prism::GetEnvOrDefault<double>("MLT_INIT_RATE", 0);
    if (init_send_rate == 0) init_send_rate = Bdp();
  }
  return init_send_rate;
}

uint64_t MLTGlobal::RateMonitorIntervalUs() {
  if (rate_monitor_interval_us == 0) {
    rate_monitor_interval_us =
        prism::GetEnvOrDefault<int>("MLT_RATE_MONITOR_INTERVAL_US", 100);
  }
  return rate_monitor_interval_us;
}

size_t MLTGlobal::ConnectionBacklogSize() {
  if (conn_backlog_size == 0) {
    conn_backlog_size =
        prism::GetEnvOrDefault<int>("MLT_CONN_BACKLOG_SIZE", 1048576);  // 1MB
  }
  return conn_backlog_size;
}