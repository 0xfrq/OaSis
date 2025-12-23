#include "keyboard.h"
#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__ (
        "inb %1, %0"
        : "=a"(ret)
        : "dN"(port)
    );
    return ret;
}

static const char keymap[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0,'\\',
    'z','x','c','v','b','n','m',',','.','/', 0,'*', 0,' ',
};

char keyboard_getchar(void) {
    uint8_t scancode;

    while (1) {
        if (!(inb(0x64) & 1))
            continue;

        scancode = inb(0x60);

        if (scancode & 0x80)
            continue;

        break;
    }

    while (1) {
        if (!(inb(0x64) & 1))
            continue;

        if (inb(0x60) & 0x80)
            break;
    }

    if (scancode == 0x0E)
        return '\b';

    return keymap[scancode];
}