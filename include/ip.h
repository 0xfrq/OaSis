#ifndef IP_H
#define IP_H

#include <stdint.h>

#define IP_PROTO_ICMP 1
#define IP_PROTO_TCP  6
#define IP_PROTO_UDP  17

/* IP header (20 bytes, no options) */
typedef struct {
    uint8_t  ver_ihl;       /* version (4) + header length (5) = 0x45 */
    uint8_t  dscp_ecn;      /* 0 */
    uint16_t total_len;     /* big-endian */
    uint16_t id;            /* identification */
    uint16_t flags_frag;    /* flags + fragment offset */
    uint8_t  ttl;           /* time to live */
    uint8_t  protocol;      /* ICMP=1, TCP=6, UDP=17 */
    uint16_t checksum;      /* header checksum */
    uint8_t  src_ip[4];
    uint8_t  dst_ip[4];
} __attribute__((packed)) ip_header_t;

#define IP_HDR_LEN 20

uint16_t ip_checksum(const uint8_t *data, int len);
int      ip_send(const uint8_t *dst_ip, uint8_t protocol, const uint8_t *data, uint16_t data_len);
void     ip_init(void);
void     ip_handle_packet(const uint8_t *packet, uint16_t len);
void     ip_register_handler(uint8_t protocol, void (*handler)(const uint8_t *src_ip, const uint8_t *pkt, uint16_t len));
void     ip_print_stats(void);

#endif
