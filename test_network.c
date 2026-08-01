#include <stdint.h>
#include <stdio.h>
#include "ip.h"
#include "ethernet.h"
#include "arp.h"

static int handled;
static uint16_t handled_len;

void eth_register_handler(uint16_t type,
                          void (*handler)(const uint8_t *, uint16_t)) {
    (void)type;
    (void)handler;
}

int arp_resolve(uint32_t ip, uint8_t *mac) {
    (void)ip;
    (void)mac;
    return 0;
}

int eth_send(const uint8_t *mac, uint16_t type,
             const uint8_t *data, uint16_t len) {
    (void)mac;
    (void)type;
    (void)data;
    (void)len;
    return 0;
}

void icmp_handle_packet(const uint8_t *src, const uint8_t *data, uint16_t len) {
    (void)src;
    (void)data;
    (void)len;
}

void vga_print(const char *text) { (void)text; }
void itoa(int value, char *out, int base) {
    (void)value;
    (void)base;
    out[0] = 0;
}

static void test_handler(const uint8_t *src, const uint8_t *data, uint16_t len) {
    (void)src;
    (void)data;
    handled++;
    handled_len = len;
}

static void expect(int condition, const char *name) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        return;
    }
    printf("ok: %s\n", name);
}

int main(void) {
    uint8_t even[] = {0x00, 0x01, 0xF2, 0x03, 0xF4, 0xF5};
    expect(ip_checksum(even, sizeof(even)) == 0x1905, "RFC 1071 checksum");

    uint8_t odd[] = {0x01, 0x02, 0x03};
    expect(ip_checksum(odd, sizeof(odd)) == 0xFBFD, "odd-length checksum");

    uint8_t packet[24] = {0x45, 0x00, 0x00, 0x18, 0, 0, 0, 0,
                          64, 99, 0, 0, 10, 0, 2, 15,
                          10, 0, 2, 2, 1, 2, 3, 4};
    ip_header_t *header = (ip_header_t *)packet;
    header->checksum = __builtin_bswap16(ip_checksum(packet, IP_HDR_LEN));
    ip_register_handler(99, test_handler);
    ip_handle_packet(packet, sizeof(packet));
    expect(handled == 1 && handled_len == 4, "valid IPv4 dispatch");

    packet[10] ^= 1;
    ip_handle_packet(packet, sizeof(packet));
    expect(handled == 1, "bad checksum rejected");

    packet[10] ^= 1;
    packet[2] = 0;
    packet[3] = 10;
    header->checksum = __builtin_bswap16(ip_checksum(packet, IP_HDR_LEN));
    ip_handle_packet(packet, sizeof(packet));
    expect(handled == 1, "truncated total length rejected");

    return 0;
}
