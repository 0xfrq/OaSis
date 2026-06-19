#include "pic.h"
#include "io.h"

#define PIC_MASTER_CMD  0x20
#define PIC_MASTER_DATA 0x21
#define PIC_SLAVE_CMD   0xA0
#define PIC_SLAVE_DATA  0xA1

#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01

// inisialisasi PIC (Programmable Interrupt Controller)
void pic_init(void) {
    // ICW1: mulai inisialisasi, butuh ICW4
    outb(PIC_MASTER_CMD, ICW1_INIT | ICW1_ICW4);
    outb(PIC_SLAVE_CMD, ICW1_INIT | ICW1_ICW4);

    // ICW2: set vektor interrupt
    // Master: IRQ 0-7 mapping ke interrupt 32-39
    // Slave: IRQ 8-15 mapping ke interrupt 40-47
    outb(PIC_MASTER_DATA, 32);
    outb(PIC_SLAVE_DATA, 40);

    // ICW3: setup cascading antara master dan slave
    // Master: IRQ 2 nyambung ke slave
    // Slave: nyambung ke master IRQ 2
    outb(PIC_MASTER_DATA, 0x04);
    outb(PIC_SLAVE_DATA, 0x02);

    // ICW4: mode 8086
    outb(PIC_MASTER_DATA, ICW4_8086);
    outb(PIC_SLAVE_DATA, ICW4_8086);

    // OCW1: matiin semua IRQ dulu di awal
    outb(PIC_MASTER_DATA, 0xFF);
    outb(PIC_SLAVE_DATA, 0xFF);
}

// aktifin satu IRQ
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

// matiin satu IRQ
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
