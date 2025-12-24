#include "pic.h"
#include "io.h"

#define PIC_MASTER_CMD  0x20
#define PIC_MASTER_DATA 0x21
#define PIC_SLAVE_CMD   0xA0
#define PIC_SLAVE_DATA  0xA1

#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01

void pic_init(void) {
    outb(PIC_MASTER_CMD, ICW1_INIT | ICW1_ICW4);
    outb(PIC_SLAVE_CMD, ICW1_INIT | ICW1_ICW4);

    // ICW2: Set interrupt vectors
    // Master: IRQ 0-7 map to interrupts 32-39
    // Slave: IRQ 8-15 map to interrupts 40-47
    outb(PIC_MASTER_DATA, 32);
    outb(PIC_SLAVE_DATA, 40);

    // ICW3: Setup cascading
    // Master: IRQ 2 connects to slave
    // Slave: Connected to master IRQ 2
    outb(PIC_MASTER_DATA, 0x04);
    outb(PIC_SLAVE_DATA, 0x02);

    // ICW4: 8086 mode
    outb(PIC_MASTER_DATA, ICW4_8086);
    outb(PIC_SLAVE_DATA, ICW4_8086);

    // OCW1: Disable all IRQs initially
    outb(PIC_MASTER_DATA, 0xFF);
    outb(PIC_SLAVE_DATA, 0xFF);
}

void pic_enable_irq(int irq) {
    uint16_t port;
    uint8_t mask;

    if (irq < 8) {
        port = PIC_MASTER_DATA;
    } else {
        port = PIC_SLAVE_DATA;
        irq -= 8;
    }

    mask = inb(port);
    mask &= ~(1 << irq);
    outb(port, mask);
}

void pic_disable_irq(int irq) {
    uint16_t port;
    uint8_t mask;

    if (irq < 8) {
        port = PIC_MASTER_DATA;
    } else {
        port = PIC_SLAVE_DATA;
        irq -= 8;
    }

    mask = inb(port);
    mask |= (1 << irq);
    outb(port, mask);
}
