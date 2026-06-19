#ifndef VGA_H
#define VGA_H
#define VGA_COLOR_BLACK 0
#define VGA_COLOR_GREEN 2
#define VGA_COLOR_YELLOW 14
#define VGA_COLOR_LIGHT_GREEN 10
#define VGA_COLOR_WHITE 15
#define VGA_COLOR_BLUE 1
#define VGA_COLOR_RED 4


#include <stdint.h>

void vga_clear(void);
void vga_putc(char c);
void vga_print(const char* str);
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_set_cursor(uint8_t x, uint8_t y);
uint8_t vga_get_cursor_x(void);
uint8_t vga_get_cursor_y(void);
void vga_write_char(uint8_t x, uint8_t y, char c, uint8_t color);
void vga_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, char c, uint8_t color);
void vga_cursor_init(void);
void vga_refresh_cursor(void);

#endif
