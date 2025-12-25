#include "io.h"


void io_wait(void) {
    /* Port 0x80 is unused and safe for delay */
    asm volatile ("outb %%al, $0x80" : : "a"(0));
}


void outb(uint16_t port, uint8_t value) {
    __asm__ __volatile__ (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__ (
        "inb %1, %0"
        : "=a"(ret)
        : "Nd"(port)
    );
    return ret;
}

void outw(uint16_t port, uint16_t value) {
    __asm__ __volatile__ (
        "outw %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ __volatile__ (
        "inw %1, %0"
        : "=a"(ret)
        : "Nd"(port)
    );
    return ret;
}