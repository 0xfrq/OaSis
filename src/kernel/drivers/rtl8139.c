#include "rtl8139.h"
#include "io.h"
#include "vga.h"
#include "string.h"
#include "paging.h"
#include "timer.h"
#include <stddef.h>

static uint16_t rtl_io_base = 0;
static uint8_t  rtl_irq     = 0;
static uint8_t  rtl_mac[6]  = {0};
static int      rtl_initialized = 0;
static uint8_t  rtl_tx_index = 0;

/* RX ring buffer (8K buffer) */
static uint8_t rtl_rx_buf[RX_BUF_SIZE] __attribute__((aligned(16)));

#define RTL_TX_DESC_COUNT 4
/* The card DMA-writes these buffers, so keep them aligned and contiguous. */
static uint8_t rtl_tx_bufs[RTL_TX_DESC_COUNT][TX_BUF_SIZE]
    __attribute__((aligned(16)));
/* current read offset in the 8 KiB RX ring (the extra 16 bytes are guard space) */
static uint32_t rtl_rx_cur = 0;

#define RTL_RX_DATA_SIZE 8192U

static uint8_t rx_byte(uint32_t offset) {
    return rtl_rx_buf[offset % RTL_RX_DATA_SIZE];
}

static uint16_t rx_word(uint32_t offset) {
    return (uint16_t)rx_byte(offset) |
           ((uint16_t)rx_byte(offset + 1) << 8);
}

static uint8_t  rb(uint16_t r) { return inb(rtl_io_base + r); }
static uint16_t rw(uint16_t r) { return inw(rtl_io_base + r); }
static uint32_t rl(uint16_t r) { return inl(rtl_io_base + r); }
static void wb(uint16_t r, uint8_t v)  { outb(rtl_io_base + r, v); }
static void ww(uint16_t r, uint16_t v) { outw(rtl_io_base + r, v); }
static void wl(uint16_t r, uint32_t v) { outl(rtl_io_base + r, v); }

void rtl8139_init(uint16_t io_base, uint8_t irq) {
    rtl_io_base = io_base;
    rtl_irq     = irq;

    vga_print("[rtl8139] Init IO=0x");
    char buf[16];
    itoa(io_base, buf, 16);
    vga_print(buf);
    vga_print(" IRQ=");
    itoa(irq, buf, 10);
    vga_print(buf);
    vga_print("\n");

    /* Reset, but never spin forever if the device is absent or wedged. */
    wb(RTL_REG_CR, RTL_CR_RST);
    uint32_t reset_start = timer_get_ticks();
    while (rb(RTL_REG_CR) & RTL_CR_RST) {
        if ((timer_get_ticks() - reset_start) > 100) {
            vga_print("[rtl8139] Reset timeout\n");
            rtl_initialized = 0;
            return;
        }
    }
    vga_print("[rtl8139] Reset OK\n");

    /* Activate 93C46 (EEPROM) — write 0 to CONFIG1 at 0x36 */
    wb(0x36, 0x00);

    /* Baca MAC */
    uint32_t mac_lo = rl(RTL_REG_IDR0);
    uint16_t mac_hi = rw(0x04);
    for (int i = 0; i < 4; i++) rtl_mac[i] = (uint8_t)(mac_lo >> (i*8));
    rtl_mac[4] = (uint8_t)(mac_hi);
    rtl_mac[5] = (uint8_t)(mac_hi >> 8);

    vga_print("[rtl8139] MAC: ");
    for (int i = 0; i < 6; i++) {
        if (rtl_mac[i] < 0x10) vga_print("0");
        itoa(rtl_mac[i], buf, 16);
        vga_print(buf);
        if (i < 5) vga_print(":");
    }
    vga_print("\n");

    /* Reset RX buffer */
    rtl_rx_cur = 0;
    for (int i = 0; i < RX_BUF_SIZE; i++) rtl_rx_buf[i] = 0;

    /* DMA registers require physical addresses, not C virtual addresses. */
    uint32_t rx_phys = virt_to_phys((uint32_t)rtl_rx_buf);
    uint32_t tx_phys = virt_to_phys((uint32_t)rtl_tx_bufs[0]);
    if (!rx_phys || !tx_phys) {
        vga_print("[rtl8139] DMA buffer is not mapped\n");
        rtl_initialized = 0;
        return;
    }

    vga_print("[rtl8139] RX phys=0x");
    itoa(rx_phys, buf, 16); vga_print(buf);
    vga_print(" TX phys=0x");
    itoa(tx_phys, buf, 16); vga_print(buf);
    vga_print("\n");

    wl(RTL_REG_RBSTART, rx_phys);

    /* RCR: 8 KiB ring, accept broadcast/multicast/our MAC. */
    wl(RTL_REG_RCR, RTL_RCR_WRAP | RTL_RCR_AB | RTL_RCR_AM | RTL_RCR_APM);
    wl(RTL_REG_TCR, 0x00000000);

    /* Clear pending interrupts before enabling the receiver. */
    ww(RTL_REG_ISR, 0xFFFF);
    ww(RTL_REG_IMR, 0x0000);

    /* CAPR is externally represented as (read_pointer - 16).  Writing zero
     * here leaves QEMU with only 16 bytes of receive space, so initialize it
     * to the wrapped value corresponding to read_pointer == 0. */
    ww(RTL_REG_CAPR, RTL_RX_DATA_SIZE - 16);
    wb(RTL_REG_CR, RTL_CR_TE | RTL_CR_RE);

    rtl_tx_index = 0;
    rtl_initialized = 1;
    vga_print("[rtl8139] Ready\n");
}

int rtl8139_send(const void *data, uint16_t len) {
    if (!rtl_initialized || !data || len == 0 || len > 1514) return -1;

    uint8_t desc = rtl_tx_index;
    uint32_t tsd = RTL_REG_TSD0 + desc * 4;
    uint32_t tsad = RTL_REG_TSAD0 + desc * 4;
    uint32_t tx_phys = virt_to_phys((uint32_t)rtl_tx_bufs[desc]);
    if (!tx_phys) return -1;

    /* A completed descriptor has TxHostOwns set.  If the previous owner has
     * not returned it yet, do not overwrite its DMA buffer. */
    uint32_t old_status = rl((uint16_t)tsd);
    if (!(old_status & RTL_TSD_OWN)) {
        vga_print("[rtl8139] TX descriptor busy\n");
        return -1;
    }

    uint16_t tx_len = (len < 60) ? 60 : len;
    const uint8_t *src = (const uint8_t *)data;
    for (uint16_t i = 0; i < len; i++) rtl_tx_bufs[desc][i] = src[i];
    for (uint16_t i = len; i < tx_len; i++) rtl_tx_bufs[desc][i] = 0;

    wl((uint16_t)tsad, tx_phys);
    /* Clear TxHostOwns by writing only the frame length; this hands the
     * descriptor to the NIC in QEMU's RTL8139 model. */
    wl((uint16_t)tsd, tx_len);

    uint32_t start = timer_get_ticks();
    /* Submission clears TxHostOwns; completion sets it again. */
    while (!(rl((uint16_t)tsd) & RTL_TSD_OWN)) {
        if ((timer_get_ticks() - start) > 10) {
            vga_print("[rtl8139] TX timeout status=0x");
            char buf[16];
            itoa(rl((uint16_t)tsd), buf, 16); vga_print(buf); vga_print("\n");
            return -1;
        }
    }

    uint32_t status = rl((uint16_t)tsd);
    if (!(status & RTL_TSD_TOK)) {
        vga_print("[rtl8139] TX error status=0x");
        char buf[16];
        itoa(status, buf, 16); vga_print(buf); vga_print("\n");
        return -1;
    }

    rtl_tx_index = (uint8_t)((desc + 1) % RTL_TX_DESC_COUNT);
    return 0;
}

int rtl8139_poll(uint8_t *buf, uint16_t *len) {
    if (!rtl_initialized || !buf || !len || *len == 0) return 0;

    /* CR.BUFE is set while the card has no complete packet for us. */
    if (rb(RTL_REG_CR) & RTL_CR_BUFE)
        return 0;

    /* RTL8139 RX header is status first, then frame length.  The length
     * includes the four-byte Ethernet CRC but not this header. */
    uint16_t status = rx_word(rtl_rx_cur);
    uint16_t pktlen = rx_word(rtl_rx_cur + 2);
    uint32_t advance = 4;

    if (!(status & RTL_RX_STATUS_ROK) || pktlen < 4 || pktlen > 1518) {
        /* A bad descriptor must still be consumed, but never trust its
         * length enough to move outside the ring. */
        rtl_rx_cur = (rtl_rx_cur + advance + 3) & ~3U;
        rtl_rx_cur %= RTL_RX_DATA_SIZE;
        ww(RTL_REG_CAPR, (uint16_t)((rtl_rx_cur + RTL_RX_DATA_SIZE - 16) % RTL_RX_DATA_SIZE));
        ww(RTL_REG_ISR, RTL_ISR_ROK);
        return 0;
    }

    uint16_t dlen = (uint16_t)(pktlen - 4);
    if (dlen > *len) dlen = *len;

    for (uint16_t i = 0; i < dlen; i++)
        buf[i] = rx_byte(rtl_rx_cur + 4 + i);
    *len = dlen;

    advance += pktlen;
    rtl_rx_cur = (rtl_rx_cur + advance + 3) & ~3U;
    rtl_rx_cur %= RTL_RX_DATA_SIZE;

    /* CAPR trails the software consumer by 16 bytes as required by the
     * controller; all offsets are kept inside the 8 KiB hardware ring. */
    ww(RTL_REG_CAPR, (uint16_t)((rtl_rx_cur + RTL_RX_DATA_SIZE - 16) % RTL_RX_DATA_SIZE));
    ww(RTL_REG_ISR, RTL_ISR_ROK);
    return 1;
}

void rtl8139_print_status(void) {
    if (!rtl_initialized) {
        vga_print("rtl8139: not initialized\n");
        return;
    }

    char buf[16];
    vga_print("RTL8139:\n");
    vga_print("  IO:0x"); itoa(rtl_io_base, buf, 16); vga_print(buf);
    vga_print(" IRQ:"); itoa(rtl_irq, buf, 10); vga_print(buf); vga_print("\n");

    vga_print("  MAC:");
    for (int i = 0; i < 6; i++) {
        if (rtl_mac[i] < 0x10) vga_print("0");
        itoa(rtl_mac[i], buf, 16); vga_print(buf);
        if (i < 5) vga_print(":");
    }
    vga_print("\n");

    vga_print("  CR:"); itoa(rb(0x37), buf, 16); vga_print(buf);
    vga_print(" ISR:"); itoa(rw(0x3E), buf, 16); vga_print(buf);
    vga_print(" CBR:"); itoa(rw(0x3A), buf, 10); vga_print(buf);
    vga_print(" RXcur:"); itoa(rtl_rx_cur, buf, 10); vga_print(buf);
    vga_print("\n");
}

uint8_t *rtl8139_get_mac(void) {
    return rtl_initialized ? rtl_mac : NULL;
}
