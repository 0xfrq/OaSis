#include "vga.h"
#include "io.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000
#define VGA_CTRL_REG 0x3D4
#define VGA_DATA_REG 0x3D5

/* Track the character currently under the cursor so we can
 * restore the cell's appearance when the cursor moves/blinks. */
static char cursor_saved_char = ' ';
static uint8_t cursor_saved_attr = 0x0F;
static int cursor_saved_valid = 0;

static uint16_t* vga_buffer = (uint16_t*)VGA_MEMORY;
static uint8_t cursor_x = 0;
static uint8_t cursor_y = 0;
static uint8_t color = 0x0F;

/* Program the VGA hardware cursor location.
 * Standard CRT controller registers: index 0x0F (low), 0x0E (high) of the cursor position. */
static void vga_hw_cursor_set(uint8_t x, uint8_t y) {
    uint16_t pos = (uint16_t)(y * VGA_WIDTH + x);
    outb(VGA_CTRL_REG, 0x0F);
    outb(VGA_DATA_REG, (uint8_t)(pos & 0xFF));
    outb(VGA_CTRL_REG, 0x0E);
    outb(VGA_DATA_REG, (uint8_t)((pos >> 8) & 0xFF));
}

/* Program the VGA hardware cursor shape: top/bottom scanline.
 * 0x0A = start scanline, 0x0B = end scanline.
 * Common text-mode blinking block: start=0, end=15 (full cell). */
static void vga_hw_cursor_shape(uint8_t start, uint8_t end) {
    /* Read current register first to preserve high bits (we only modify low 5 bits) */
    outb(VGA_CTRL_REG, 0x0A);
    uint8_t cur_start = inb(VGA_DATA_REG);
    outb(VGA_DATA_REG, (uint8_t)((cur_start & 0xC0) | (start & 0x1F)));

    outb(VGA_CTRL_REG, 0x0B);
    uint8_t cur_end = inb(VGA_DATA_REG);
    outb(VGA_DATA_REG, (uint8_t)((cur_end & 0xE0) | (end & 0x1F)));
}

/* Save the character currently at the cursor position so we can
 * restore it when the cursor moves. */
static void vga_save_cell(void) {
    if (cursor_saved_valid) {
        /* already saved, do nothing */
        return;
    }
    uint16_t cell = vga_buffer[cursor_y * VGA_WIDTH + cursor_x];
    cursor_saved_char = (char)(cell & 0xFF);
    cursor_saved_attr = (uint8_t)((cell >> 8) & 0xFF);
    cursor_saved_valid = 1;
}

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | (uint16_t)color << 8;
}

static void vga_scroll(void) {
    for(int y=1;y<VGA_HEIGHT; y++) {
        for(int x=0; x<VGA_WIDTH; x++) {
            vga_buffer[(y-1)*VGA_WIDTH+x] = vga_buffer[y*VGA_WIDTH + x];
        }
    }

    for(int x=0;x<VGA_WIDTH;x++) {
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
    vga_hw_cursor_set(cursor_x, cursor_y);
}

void vga_putc(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
        if (cursor_y >= VGA_HEIGHT)
            vga_scroll();
        vga_hw_cursor_set(cursor_x, cursor_y);
        return;
    }

    if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            vga_buffer[cursor_y * VGA_WIDTH + cursor_x] = vga_entry(' ', color);
        }
        vga_hw_cursor_set(cursor_x, cursor_y);
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
    vga_hw_cursor_set(cursor_x, cursor_y);
}

void vga_print(const char* str) {
    for (int i = 0; str[i]; i++) {
        vga_putc(str[i]);
    }
}


void vga_set_color(uint8_t fg, uint8_t bg) {
    color = fg | (bg << 4);
}

void vga_set_cursor(uint8_t x, uint8_t y) {
    if (x < VGA_WIDTH) cursor_x = x;
    if (y < VGA_HEIGHT) cursor_y = y;
    /* Move hardware cursor too */
    vga_hw_cursor_set(cursor_x, cursor_y);
}

uint8_t vga_get_cursor_x(void) {
    return cursor_x;
}

uint8_t vga_get_cursor_y(void) {
    return cursor_y;
}

/* Force-refresh the hardware cursor at the current software cursor position.
 * Called after any operation that might have invalidated the cursor (clear, etc.). */
void vga_refresh_cursor(void) {
    vga_hw_cursor_set(cursor_x, cursor_y);
}

/* Initialize VGA hardware cursor shape (full blinking block). */
void vga_cursor_init(void) {
    vga_hw_cursor_shape(0, 15);  /* full cell */
    vga_hw_cursor_set(cursor_x, cursor_y);
}

void vga_write_char(uint8_t x, uint8_t y, char c, uint8_t color) {
    if (x < VGA_WIDTH && y < VGA_HEIGHT) {
        vga_buffer[y * VGA_WIDTH + x] = vga_entry(c, color);
    }
}

void vga_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, char c, uint8_t color) {
    for (uint8_t row = y; row < y + h && row < VGA_HEIGHT; row++) {
        for (uint8_t col = x; col < x + w && col < VGA_WIDTH; col++) {
            vga_buffer[row * VGA_WIDTH + col] = vga_entry(c, color);
        }
    }
}
