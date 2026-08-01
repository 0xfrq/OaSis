#ifndef ARP_H
#define ARP_H

#include <stdint.h>

#define ARP_HW_ETHERNET 1
#define ARP_OP_REQUEST  1
#define ARP_OP_REPLY    2

/* ARP packet (28 bytes after ethernet header) */
typedef struct {
    uint16_t hw_type;       /* 1 = ethernet (big-endian) */
    uint16_t proto_type;    /* 0x0800 = IPv4 (big-endian) */
    uint8_t  hw_len;        /* 6 */
    uint8_t  proto_len;     /* 4 */
    uint16_t opcode;        /* 1=request, 2=reply (big-endian) */
    uint8_t  src_mac[6];
    uint8_t  src_ip[4];
    uint8_t  dst_mac[6];
    uint8_t  dst_ip[4];
} __attribute__((packed)) arp_packet_t;

/* ARP cache entry */
typedef struct {
    uint8_t  ip[4];
    uint8_t  mac[6];
    int      valid;
} arp_cache_t;

#define ARP_CACHE_SIZE 8

void   arp_init(void);
/* Resolve IP → MAC: return 1 if found, 0 if not.
 * If cache miss, sends ARP request and polls briefly. */
int    arp_resolve(uint32_t ip, uint8_t *mac_out);
void   arp_handle_packet(arp_packet_t *arp, uint16_t len);
void   arp_print_cache(void);

#endif
