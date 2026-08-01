#ifndef ICMP_H
#define ICMP_H

#include <stdint.h>

#define ICMP_TYPE_ECHO_REPLY   0
#define ICMP_TYPE_ECHO_REQUEST 8

typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
    uint8_t  data[];  /* variable-length payload */
} __attribute__((packed)) icmp_header_t;

void icmp_handle_packet(const uint8_t *src_ip, const uint8_t *data, uint16_t len);

#endif
