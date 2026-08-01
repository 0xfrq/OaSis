#include "ethernet.h"
#include "rtl8139.h"
#include "arp.h"
#include "vga.h"
#include "string.h"
#include <stddef.h>

const uint8_t eth_broadcast[ETH_ADDR_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const uint8_t eth_null_addr[ETH_ADDR_LEN] = {0, 0, 0, 0, 0, 0};

/* Registered protocol handlers */
#define MAX_HANDLERS 4
static struct {
    uint16_t eth_type;
    void (*handler)(const uint8_t *frame, uint16_t len);
} handlers[MAX_HANDLERS];
static int handler_count = 0;

/* Our MAC address */
static uint8_t our_mac[ETH_ADDR_LEN] = {0};

int eth_init(void) {
    uint8_t *mac = rtl8139_get_mac();
    if (!mac) {
        vga_print("eth: no NIC available\n");
        return -1;
    }
    for (int i = 0; i < ETH_ADDR_LEN; i++)
        our_mac[i] = mac[i];

    vga_print("[eth] Initialized, MAC: ");
    char buf[16];
    for (int i = 0; i < ETH_ADDR_LEN; i++) {
        if (our_mac[i] < 0x10) vga_print("0");
        itoa(our_mac[i], buf, 16);
        vga_print(buf);
        if (i < 5) vga_print(":");
    }
    vga_print("\n");

    /* Register ARP handler */
    eth_register_handler(ETH_TYPE_ARP, (void (*)(const uint8_t *, uint16_t))arp_handle_packet);

    return 0;
}

int eth_send(const uint8_t *dst_mac, uint16_t eth_type, const uint8_t *data, uint16_t len) {
    if (!dst_mac || !data || len > 1500) return -1;

    /* Build frame buffer on stack (max Ethernet MTU). */
    uint8_t frame[1514];
    eth_header_t *eth = (eth_header_t *)frame;

    for (int i = 0; i < ETH_ADDR_LEN; i++) {
        eth->dst[i] = dst_mac[i];
        eth->src[i] = our_mac[i];
    }
    eth->type = __builtin_bswap16(eth_type);

    for (uint16_t i = 0; i < len; i++)
        frame[14 + i] = data[i];

    return rtl8139_send(frame, 14 + len);
}

void eth_dispatch(void) {
    uint8_t buf[1514];
    uint16_t len;

    /* Loop until RX buffer empty */
    while (1) {
        len = 1514;
        if (!rtl8139_poll(buf, &len))
            break;

        if (len < 14) continue;  /* too small */

        eth_header_t *eth = (eth_header_t *)buf;
        uint16_t eth_type = __builtin_bswap16(eth->type);

        /* The NIC normally filters this, but retain the check for drivers or
         * future promiscuous mode. */
        int for_us = 1;
        for (int i = 0; i < ETH_ADDR_LEN; i++) {
            if (eth->dst[i] != our_mac[i]) {
                for_us = 0;
                break;
            }
        }
        if (!for_us) {
            for_us = 1;
            for (int i = 0; i < ETH_ADDR_LEN; i++) {
                if (eth->dst[i] != eth_broadcast[i]) {
                    for_us = 0;
                    break;
                }
            }
        }
        if (!for_us) continue;

        char dbg[16];
        itoa(eth_type, dbg, 16);
        vga_print("[eth] Rx type=0x");
        vga_print(dbg);
        vga_print(" len=");
        itoa(len, dbg, 10);
        vga_print(dbg);
        vga_print("\n");

        if (len < sizeof(eth_header_t)) continue;

        /* Dispatch to handler */
        for (int i = 0; i < handler_count; i++) {
            if (handlers[i].eth_type == eth_type) {
                handlers[i].handler(buf + 14, len - 14);
                break;
            }
        }
    }
}

void eth_register_handler(uint16_t eth_type, void (*handler)(const uint8_t *, uint16_t)) {
    if (handler_count >= MAX_HANDLERS) return;
    handlers[handler_count].eth_type = eth_type;
    handlers[handler_count].handler  = handler;
    handler_count++;
}

int eth_is_link_up(void) {
    return (rtl8139_get_mac() != NULL);
}
