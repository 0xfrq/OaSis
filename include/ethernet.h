#ifndef ETHERNET_H
#define ETHERNET_H

#include <stdint.h>

#define ETH_ADDR_LEN 6
#define ETH_TYPE_ARP  0x0806
#define ETH_TYPE_IP   0x0800

/* Broadcast MAC */
extern const uint8_t eth_broadcast[ETH_ADDR_LEN];
extern const uint8_t eth_null_addr[ETH_ADDR_LEN];

/* Ethernet header — 14 bytes */
typedef struct {
    uint8_t  dst[ETH_ADDR_LEN];
    uint8_t  src[ETH_ADDR_LEN];
    uint16_t type;  /* big-endian */
} __attribute__((packed)) eth_header_t;

int  eth_init(void);
int  eth_send(const uint8_t *dst_mac, uint16_t eth_type, const uint8_t *data, uint16_t len);
void eth_dispatch(void);  /* dipanggil dari loop — poll + dispatch ke handler */
void eth_register_handler(uint16_t eth_type, void (*handler)(const uint8_t *frame, uint16_t len));
int  eth_is_link_up(void);

#endif
