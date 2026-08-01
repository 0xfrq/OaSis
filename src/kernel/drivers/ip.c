#include "ip.h"
#include "icmp.h"
#include "netcfg.h"
#include "ethernet.h"
#include "arp.h"
#include "vga.h"
#include "string.h"
#include <stddef.h>

static uint16_t ip_packet_id = 0;

/* Registered protocol handlers */
#define MAX_IP_HANDLERS 4
static struct {
    uint8_t protocol;
    void (*handler)(const uint8_t *src_ip, const uint8_t *pkt, uint16_t len);
} ip_handlers[MAX_IP_HANDLERS];
static int ip_handler_count = 0;

uint16_t ip_checksum(const uint8_t *data, int len) {
    uint32_t sum = 0;
    for (int i = 0; i < len; i += 2) {
        uint16_t word = (uint16_t)data[i] << 8;
        if (i + 1 < len) word |= data[i + 1];
        sum += word;
    }
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum & 0xFFFF);
}

static void ip_build_header(ip_header_t *hdr, const uint8_t *dst_ip, uint8_t protocol, uint16_t total_len) {
    uint8_t our_ip[4];
    net_get_ip(our_ip);

    hdr->ver_ihl    = 0x45;          /* IPv4, 20 byte header */
    hdr->dscp_ecn   = 0;
    hdr->total_len  = __builtin_bswap16(total_len);
    hdr->id         = __builtin_bswap16(ip_packet_id++);
    hdr->flags_frag = 0;
    hdr->ttl        = 64;
    hdr->protocol   = protocol;
    hdr->checksum   = 0;  /* compute below */

    for (int i = 0; i < 4; i++) {
        hdr->src_ip[i] = our_ip[i];
        hdr->dst_ip[i] = dst_ip[i];
    }

    hdr->checksum = __builtin_bswap16(ip_checksum((uint8_t *)hdr, IP_HDR_LEN));
}

int ip_send(const uint8_t *dst_ip, uint8_t protocol, const uint8_t *data, uint16_t data_len) {
    /* Build IP packet (header + payload) on stack */
    uint8_t pkt[IP_HDR_LEN + 1500];
    uint16_t total_len = IP_HDR_LEN + data_len;

    ip_header_t *hdr = (ip_header_t *)pkt;
    ip_build_header(hdr, dst_ip, protocol, total_len);

    for (uint16_t i = 0; i < data_len; i++)
        pkt[IP_HDR_LEN + i] = data[i];

    /* Resolve MAC via ARP, then send via ethernet */
    uint32_t dst_ip_int = ((uint32_t)dst_ip[0] << 24) |
                          ((uint32_t)dst_ip[1] << 16) |
                          ((uint32_t)dst_ip[2] << 8)  |
                          dst_ip[3];

    uint8_t dst_mac[6];
    if (!arp_resolve(dst_ip_int, dst_mac)) {
        vga_print("ip: ARP resolve failed\n");
        return -1;
    }

    return eth_send(dst_mac, ETH_TYPE_IP, pkt, total_len);
}

void ip_register_handler(uint8_t protocol, void (*handler)(const uint8_t *, const uint8_t *, uint16_t)) {
    if (ip_handler_count >= MAX_IP_HANDLERS) return;
    ip_handlers[ip_handler_count].protocol = protocol;
    ip_handlers[ip_handler_count].handler  = handler;
    ip_handler_count++;
}

void ip_handle_packet(const uint8_t *packet, uint16_t len) {
    if (!packet || len < IP_HDR_LEN) return;

    ip_header_t *hdr = (ip_header_t *)packet;
    if ((hdr->ver_ihl >> 4) != 4) return;
    int hdr_len = (hdr->ver_ihl & 0x0F) * 4;
    if (hdr_len < IP_HDR_LEN || hdr_len > len) return;

    uint16_t total_len = __builtin_bswap16(hdr->total_len);
    if (total_len < (uint16_t)hdr_len || total_len > len) return;

    /* A valid network-order header sums to zero. */
    if (ip_checksum(packet, hdr_len) != 0) return;

    uint8_t protocol = hdr->protocol;
    uint16_t payload_len = (uint16_t)(total_len - hdr_len);

    for (int i = 0; i < ip_handler_count; i++) {
        if (ip_handlers[i].protocol == protocol) {
            ip_handlers[i].handler(hdr->src_ip, packet + hdr_len, payload_len);
            return;
        }
    }
}

void ip_init(void) {
    /* Register ICMP handler */
    ip_register_handler(IP_PROTO_ICMP, icmp_handle_packet);
    /* Register IP handler on ethernet */
    eth_register_handler(ETH_TYPE_IP, ip_handle_packet);

    char buf[16];
    uint8_t our_ip[4];
    net_get_ip(our_ip);
    vga_print("[ip] Initialized, IP: ");
    itoa(our_ip[0], buf, 10); vga_print(buf); vga_print(".");
    itoa(our_ip[1], buf, 10); vga_print(buf); vga_print(".");
    itoa(our_ip[2], buf, 10); vga_print(buf); vga_print(".");
    itoa(our_ip[3], buf, 10); vga_print(buf);
    vga_print("\n");
}

void ip_print_stats(void) {
    vga_print("IP packets sent: ...\n");
}
