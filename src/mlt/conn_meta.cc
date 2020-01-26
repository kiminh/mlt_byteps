#include "conn_meta.h"
#include "mlt_global.h"

ConnMeta::ConnMeta(int dest)
    : dest_comm_id{dest},
      tx_meter{MLTGlobal::Get()->RateMonitorIntervalUs()},
      rx_meter{MLTGlobal::Get()->RateMonitorIntervalUs()} {
  send_window.store(MLTGlobal::Get()->InitialSendingWindow());
  sending_rate.store(MLTGlobal::Get()->InitialSendingRate());
  InitBacklog();
}

void ConnMeta::InitBacklog() {
  backlog_buffer_size = MLTGlobal::Get()->ConnectionBacklogSize();
  if (prism::GetEnvOrDefault<int>("MLT_DISABLE_FLOW_BACKLOG", 1) != 0)
    return;

  backlog_buffer = std::unique_ptr<char[]>(new char[backlog_buffer_size]);
  char* ptr = backlog_buffer.get();
  while (ptr < ptr + backlog_buffer_size) {
    backlog_free_list.emplace_back(reinterpret_cast<GradPacket*>(ptr));
    ptr += MLTGlobal::Get()->MaxSegment();
  }
}