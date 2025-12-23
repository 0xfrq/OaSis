#ifndef VGA_H
#define VGA_H
#define VGA_COLOR_BLACK 0
#define VGA_COLOR_GREEN 2
#define VGA_COLOR_LIGHT_GREEN 10


#include <stdint.h>

void vga_clear(void);
void vga_putc(char c);
void vga_print(const char* str);
void vga_set_color(uint8_t fg, uint8_t bg);

#endif