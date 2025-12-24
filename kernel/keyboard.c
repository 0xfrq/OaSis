#include "keyboard.h"
#include "io.h"
#include "vga.h"
#include <stdint.h>

#define KEYBOARD_DATA 0x60
#define KEYBOARD_BUFFER_SIZE 256

static const char keymap[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0,'\\',
    'z','x','c','v','b','n','m',',','.','/', 0,'*', 0,' ',
};

static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static int read_pos = 0;
static int write_pos = 0;

void keyboard_init(void) {
}

void keyboard_interrupt_handler(void) {
    uint8_t scancode = inb(KEYBOARD_DATA);

    if (scancode & 0x80) {
        return;
    }

    char c = keymap[scancode];
    
    if (c != 0) {
        keyboard_buffer[write_pos] = c;
        write_pos = (write_pos + 1) % KEYBOARD_BUFFER_SIZE;
        
    }
}

char keyboard_getchar(void) {
    while (read_pos == write_pos) {
        asm("hlt");  
    }

    char c = keyboard_buffer[read_pos];
    read_pos = (read_pos + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}