#include "vga.h"
#include "keyboard.h"
#include "io.h"

// void vga_clear(void);
// void vga_print(const char* str);
// void vga_putc(char c);
// char keyboard_getchar(void);

void kernel_main(void) {
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);

    vga_clear();
    vga_print("OASIS kernel booted successfully\n");
    vga_print("> ");

    while (1) {
        char c = keyboard_getchar();
        vga_putc(c);
    }
}
