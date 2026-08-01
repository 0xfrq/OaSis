#include "arp.h"
#include "ethernet.h"
#include "rtl8139.h"
#include "netcfg.h"
#include "vga.h"
#include "string.h"
#include <stddef.h>

static arp_cache_t arp_cache[ARP_CACHE_SIZE];

/* Helper: read uint16 big-endian from packet */
static uint16_t r16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] << 8) | p[1];
}
static void w16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFF);
}

void arp_init(void) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++)
        arp_cache[i].valid = 0;
}

static int arp_cache_lookup(const uint8_t *ip, uint8_t *mac_out) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid &&
            arp_cache[i].ip[0] == ip[0] &&
            arp_cache[i].ip[1] == ip[1] &&
            arp_cache[i].ip[2] == ip[2] &&
            arp_cache[i].ip[3] == ip[3]) {
            for (int j = 0; j < ETH_ADDR_LEN; j++)
                mac_out[j] = arp_cache[i].mac[j];
            return 1;
        }
    }
    return 0;
}

static void arp_cache_add(const uint8_t *ip, const uint8_t *mac) {
    /* Replace existing or find free slot */
    int slot = -1;
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid &&
            arp_cache[i].ip[0] == ip[0] &&
            arp_cache[i].ip[1] == ip[1] &&
            arp_cache[i].ip[2] == ip[2] &&
            arp_cache[i].ip[3] == ip[3]) {
            slot = i;
            break;
        }
        if (!arp_cache[i].valid && slot < 0)
            slot = i;
    }
    if (slot < 0) {
        /* Evict oldest (slot 0) */
        for (int i = 1; i < ARP_CACHE_SIZE; i++)
            arp_cache[i-1] = arp_cache[i];
        slot = ARP_CACHE_SIZE - 1;
    }

    for (int i = 0; i < 4; i++) arp_cache[slot].ip[i] = ip[i];
    for (int i = 0; i < ETH_ADDR_LEN; i++) arp_cache[slot].mac[i] = mac[i];
    arp_cache[slot].valid = 1;
}

#include "timer.h"

int arp_resolve(uint32_t ip, uint8_t *mac_out) {
    uint8_t ip_bytes[4];
    ip_bytes[0] = (uint8_t)(ip >> 24);
    ip_bytes[1] = (uint8_t)(ip >> 16);
    ip_bytes[2] = (uint8_t)(ip >> 8);
    ip_bytes[3] = (uint8_t)(ip);

    if (arp_cache_lookup(ip_bytes, mac_out))
        return 1;

    /* ARP request broadcast */
    uint8_t arp_pkt[28];
    uint8_t our_mac[6];
    uint8_t *m = rtl8139_get_mac();
    if (!m) return 0;
    for (int i = 0; i < 6; i++) our_mac[i] = m[i];

    w16(arp_pkt + 0, ARP_HW_ETHERNET);       /* hw_type */
    w16(arp_pkt + 2, 0x0800);                 /* proto_type */
    arp_pkt[4] = 6;                            /* hw_len */
    arp_pkt[5] = 4;                            /* proto_len */
    w16(arp_pkt + 6, ARP_OP_REQUEST);          /* opcode */
    for (int i = 0; i < 6; i++) arp_pkt[8 + i] = our_mac[i];   /* src_mac */
    {
        uint8_t our_ip[4];
        net_get_ip(our_ip);
        for (int i = 0; i < 4; i++) arp_pkt[14 + i] = our_ip[i];  /* our IP */
    }
    for (int i = 0; i < 6; i++) arp_pkt[18 + i] = 0;           /* dst_mac */
    for (int i = 0; i < 4; i++) arp_pkt[24 + i] = ip_bytes[i]; /* dst_ip */

    if (eth_send(eth_broadcast, ETH_TYPE_ARP, arp_pkt, 28) < 0) {
        vga_print("arp: request transmit failed\n");
        return 0;
    }

    /* Poll for reply (wait up to ~500ms) */
    uint32_t start = timer_get_ticks();
    while ((timer_get_ticks() - start) < 50) {  /* 50 ticks = 500ms */
        eth_dispatch();
        if (arp_cache_lookup(ip_bytes, mac_out))
            return 1;
    }

    return 0;  /* not resolved */
}

void arp_handle_packet(arp_packet_t *arp, uint16_t len) {
    (void)len;
    if (len < sizeof(arp_packet_t)) return;
    if (r16((uint8_t *)&arp->hw_type) != ARP_HW_ETHERNET) return;
    if (arp->hw_len != 6 || arp->proto_len != 4) return;

    vga_print("[arp] Packet rcvd! OP=");
    char dbg[16];
    itoa(r16((uint8_t *)&arp->opcode), dbg, 10);
    vga_print(dbg);
    vga_print("\n");

    /* We add sender to cache regardless of opcode */
    arp_cache_add(arp->src_ip, arp->src_mac);

    uint16_t op = r16((uint8_t *)&arp->opcode);
    if (op == ARP_OP_REQUEST) {
        uint8_t our_ip[4];
        net_get_ip(our_ip);
        if (arp->dst_ip[0] == our_ip[0] && arp->dst_ip[1] == our_ip[1] &&
            arp->dst_ip[2] == our_ip[2] && arp->dst_ip[3] == our_ip[3]) {
            uint8_t reply[28];
            uint8_t *our_mac = rtl8139_get_mac();
            w16(reply + 0, ARP_HW_ETHERNET);
            w16(reply + 2, 0x0800);
            reply[4] = 6; reply[5] = 4;
            w16(reply + 6, ARP_OP_REPLY);
            for (int i = 0; i < 6; i++) reply[8 + i] = our_mac[i];
            for (int i = 0; i < 4; i++) reply[14 + i] = our_ip[i];
            for (int i = 0; i < 6; i++) reply[18 + i] = arp->src_mac[i];
            for (int i = 0; i < 4; i++) reply[24 + i] = arp->src_ip[i];
            eth_send(arp->src_mac, ETH_TYPE_ARP, reply, 28);
        }
    }
}

void arp_print_cache(void) {
    char buf[16];
    vga_print("ARP cache:\n");
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) continue;
        itoa(arp_cache[i].ip[0], buf, 10); vga_print(buf); vga_print(".");
        itoa(arp_cache[i].ip[1], buf, 10); vga_print(buf); vga_print(".");
        itoa(arp_cache[i].ip[2], buf, 10); vga_print(buf); vga_print(".");
        itoa(arp_cache[i].ip[3], buf, 10); vga_print(buf);
        vga_print(" -> ");
        for (int j = 0; j < ETH_ADDR_LEN; j++) {
            if (arp_cache[i].mac[j] < 0x10) vga_print("0");
            itoa(arp_cache[i].mac[j], buf, 16);
            vga_print(buf);
            if (j < 5) vga_print(":");
        }
        vga_print("\n");
    }
}
