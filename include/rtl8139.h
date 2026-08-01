#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>

/* I/O register offsets (I/O space via BAR0) */
#define RTL_REG_IDR0     0x00  /* MAC address (6 bytes) */
#define RTL_REG_IDR4     0x04  /* MAC address high */
#define RTL_REG_RBSTART  0x30  /* RX buffer start address (32-bit) */
#define RTL_REG_CR       0x37  /* Command Register (8-bit) */
#define RTL_REG_CAPR     0x38  /* Current Address of Packet Read (16-bit) */
#define RTL_REG_IMR      0x3C  /* Interrupt Mask Register (16-bit) */
#define RTL_REG_ISR      0x3E  /* Interrupt Status Register (16-bit) */
#define RTL_REG_TCR      0x40  /* TX Configuration (32-bit) */
#define RTL_REG_RCR      0x44  /* RX Configuration (32-bit) */
#define RTL_REG_TSD0     0x10  /* TX Status Descriptor 0 (32-bit) */
#define RTL_REG_TSAD0    0x20  /* TX Start Address 0 (32-bit) */

/* Command Register bits */
#define RTL_CR_RST       (1 << 4)  /* Reset */
#define RTL_CR_RE        (1 << 3)  /* Receiver Enable */
#define RTL_CR_TE        (1 << 2)  /* Transmitter Enable */
#define RTL_CR_BUFE      (1 << 0)  /* Buffer Empty */

/* RX Configuration bits (RCR register at 0x44) */
#define RTL_RCR_WRAP     (1 << 7)  /* Wrap (ring buffer mode) */
#define RTL_RCR_AB       (1 << 3)  /* Accept Broadcast */
#define RTL_RCR_AM       (1 << 2)  /* Accept Multicast */
#define RTL_RCR_APM      (1 << 1)  /* Accept Physical Match */
#define RTL_RCR_AAP      (1 << 0)  /* Accept All Packets (promiscuous) */

/* TX Status bits */
#define RTL_TSD_TOK      (1 << 15) /* Transmit OK */
#define RTL_TSD_TUN      (1 << 14) /* Transmit Underrun */
#define RTL_TSD_OWN      (1 << 13) /* DMA complete */
#define RTL_TSD_SIZE_MASK 0x1FFF   /* Packet size mask */
#define RTL_RX_STATUS_ROK  (1 << 0) /* RX descriptor: receive OK */

/* Interrupt bits */
#define RTL_ISR_ROK      (1 << 0)  /* Receive OK */
#define RTL_ISR_TOK      (1 << 1)  /* Transmit OK */
#define RTL_ISR_ERR      (1 << 2)  /* Error */
#define RTL_ISR_RX_ERR   (1 << 3)  /* Receive Error */
#define RTL_ISR_TX_ERR   (1 << 6)  /* Transmit Error */

#define RTL8139_IO_SIZE  0x100     /* I/O space size */
/* RTL8139 needs room for the ring, its 16-byte guard area, and a
 * maximum-size frame when WRAP is enabled. */
#define RTL_RX_RING_SIZE (8192 + 16)
#define RX_BUF_SIZE      RTL_RX_RING_SIZE
#define TX_BUF_SIZE      1536      /* TX buffer size (max ethernet packet) */

void rtl8139_init(uint16_t io_base, uint8_t irq);
int  rtl8139_send(const void *data, uint16_t len);
int  rtl8139_poll(uint8_t *buf, uint16_t *len);
void rtl8139_print_status(void);
uint8_t *rtl8139_get_mac(void);

#endif
