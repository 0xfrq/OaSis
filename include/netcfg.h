#ifndef NETCFG_H
#define NETCFG_H

#include <stdint.h>

/* QEMU user-mode networking default */
#define NET_OUR_IP      0x0A00020F  /* 10.0.2.15 */
#define NET_GATEWAY     0x0A000201  /* 10.0.2.1  */
#define NET_NETMASK     0xFFFFFF00  /* 255.255.255.0 */
#define NET_DNS         0x0A000203  /* 10.0.2.3  */

/* Get our IP as a byte array */
static inline void net_get_ip(uint8_t *ip) {
    uint32_t nip = NET_OUR_IP;
    ip[0] = (uint8_t)(nip >> 24);
    ip[1] = (uint8_t)(nip >> 16);
    ip[2] = (uint8_t)(nip >> 8);
    ip[3] = (uint8_t)(nip);
}

#endif
