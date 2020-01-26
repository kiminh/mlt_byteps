#ifndef PACKET_H_
#define PACKET_H_
#include <stdint.h>

struct Packet {
  uint64_t psn;
  uint16_t payload_len;
} __attribute__ ((packed));


#endif
