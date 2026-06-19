#include "io.h"


// delay bentar pake port 0x80 yang gak kepake
void io_wait(void) {
    /* port 0x80 aman dipake buat delay */
    asm volatile ("outb %%al, $0x80" : : "a"(0));
}


// kirim 1 byte ke port
void outb(uint16_t port, uint8_t value) {
    __asm__ __volatile__ (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

// baca 1 byte dari port
uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__ (
        "inb %1, %0"
        : "=a"(ret)
        : "Nd"(port)
    );
    return ret;
}

// kirim 1 word ke port
void outw(uint16_t port, uint16_t value) {
    __asm__ __volatile__ (
        "outw %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}

// baca 1 word dari port
uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ __volatile__ (
        "inw %1, %0"
        : "=a"(ret)
        : "Nd"(port)
    );
    return ret;
}
