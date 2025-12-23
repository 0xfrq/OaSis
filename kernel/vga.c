#include "vga.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

static uint16_t* vga_buffer = (uint16_t*)VGA_MEMORY;
static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;
static uint8_t color = 0x0F;

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | (uint16_t)color << 8;
}

static void vga_scroll(void) {
    for(int y=1;y<VGA_WIDTH; y++) {
        for(int x=0; x<VGA_WIDTH; x++) {
            vga_buffer[(y-1)*VGA_WIDTH+x] = vga_buffer[y*VGA_WIDTH + x];
        }
    }

    for(int x=0;x<VGA_HEIGHT;x++) {
        vga_buffer[(VGA_HEIGHT - 1)*VGA_WIDTH+x] = vga_entry(' ', color);
    }

    cursor_y = VGA_HEIGHT-1;
}

void vga_clear(void) {
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[y * VGA_WIDTH + x] = vga_entry(' ', color);
        }
    }
    cursor_x = 0;
    cursor_y = 0;
}

void vga_putc(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= VGA_HEIGHT)
            vga_scroll();
        return;
    }

    if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(' ', color);
        }
        return;
    }


    vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(c, color);

    cursor_x++;

    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= VGA_HEIGHT)
            vga_scroll();
    }
}

void vga_print(const char* str) {
    for (int i = 0; str[i]; i++) {
        vga_putc(str[i]);
    }
}


void vga_set_color(uint8_t fg, uint8_t bg) {
    color = fg | (bg << 4);
}
