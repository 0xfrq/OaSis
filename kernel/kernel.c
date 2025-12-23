#include "vga.h"
#include "keyboard.h"
#include "io.h"
#include "string.h"

// void vga_clear(void);
// void vga_print(const char* str);
// void vga_putc(char c);
// char keyboard_getchar(void);

#define INPUT_MAX 128

void kernel_main(void) {
    outb(0x21, 0xFF);
    outb(0xA1, 0xFF);

    vga_clear();
    vga_print("OASIS kernel booted successfully\n");
    vga_print("type 'help' for commands\n\n");
    char input[INPUT_MAX];
    int index = 0;
    
    vga_print("> ");

    while (1) {
        char c = keyboard_getchar();

        if(c=='\n') {
            input[index] = 0;
            vga_putc('\n');

            if(strcmp(input, "help") == 0) {
                vga_print("commands:\n");
                vga_print("     help - show this message\n");
                vga_print("     clear - clear screen\n");
            } else if (strcmp(input, "clear") == 0) {
                vga_clear();
            } else if (index != 0) {
                vga_print("unknown command, nulis yg bener\n");
            }

            index = 0;
            vga_print("> ");
            continue;
        }

        if(c=='\b') {
            if(index > 0) {
                index--;
                vga_putc('\b');
            }
            continue;
        }
        if(index < INPUT_MAX -1) {
            input[index++] = c;
            vga_putc(c);
        }
    }
}
