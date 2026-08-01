#include "icmp.h"
#include "ip.h"
#include "vga.h"
#include "string.h"
#include "rtl8139.h"
#include "netcfg.h"
#include <stddef.h>

static uint16_t icmp_checksum(const uint8_t *data, int len) {
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

void icmp_handle_packet(const uint8_t *src_ip, const uint8_t *data, uint16_t len) {
    if (!src_ip || !data || len < sizeof(icmp_header_t)) return;

    /* Validate before dispatching an echo reply or reflecting a request. */
    if (icmp_checksum(data, len) != 0) return;

    icmp_header_t *icmp = (icmp_header_t *)data;

    if (icmp->type == ICMP_TYPE_ECHO_REPLY) {
        /* Incoming echo reply — print it for the ping command */
        char buf[16];
        vga_print("Reply from ");
        itoa(src_ip[0], buf, 10); vga_print(buf); vga_print(".");
        itoa(src_ip[1], buf, 10); vga_print(buf); vga_print(".");
        itoa(src_ip[2], buf, 10); vga_print(buf); vga_print(".");
        itoa(src_ip[3], buf, 10); vga_print(buf);
        vga_print(": icmp_seq=");
        itoa(__builtin_bswap16(icmp->seq), buf, 10);
        vga_print(buf);
        vga_print(" ttl=64\n");
    } else if (icmp->type == ICMP_TYPE_ECHO_REQUEST) {
        uint16_t payload_len = len - sizeof(icmp_header_t);
        uint16_t reply_len   = sizeof(icmp_header_t) + payload_len;

        uint8_t reply[reply_len];
        icmp_header_t *resp = (icmp_header_t *)reply;

        resp->type     = ICMP_TYPE_ECHO_REPLY;
        resp->code     = 0;
        resp->checksum = 0;
        resp->id       = icmp->id;
        resp->seq      = icmp->seq;

        for (uint16_t i = 0; i < payload_len; i++)
            reply[sizeof(icmp_header_t) + i] = data[sizeof(icmp_header_t) + i];

        resp->checksum = icmp_checksum(reply, reply_len);
        ip_send(src_ip, IP_PROTO_ICMP, reply, reply_len);
    }
}
