---
layout: default
title: driver
---

# Drivers

This section documents the hardware boundary of OaSis. The [networking internals](networking/) page covers the complete PCI-to-ICMP path; the pages below cover the earlier device drivers.

## Driver catalog

| Driver | Status | Source |
| --- | --- | --- |
| PCI configuration space | Implemented for bus 0 | `src/kernel/drivers/pci.c` |
| RTL8139 Ethernet | Implemented for QEMU, polling mode | `src/kernel/drivers/rtl8139.c` |
| Ethernet, ARP, IPv4, ICMP | Implemented subset | `src/kernel/drivers/ethernet.c`, `arp.c`, `ip.c`, `icmp.c` |
| PS/2 keyboard | Implemented | `src/kernel/drivers/keyboard.c` |
| PIT timer | Implemented | `src/kernel/drivers/timer.c` |
| VGA text mode | Implemented | `src/kernel/core/vga.c` |
| ATA PIO and block cache | Implemented | `src/kernel/drivers/ata.c`, `block.c` |
| PIC and port I/O | Implemented | `src/kernel/drivers/pic.c`, `io.c` |

## Networking

Read [Networking internals](networking/) for PCI discovery, RTL8139 DMA, Ethernet frames, ARP resolution, IPv4 validation, ICMP echo, QEMU setup, and troubleshooting.

## Legacy driver details

The sections below retain the detailed keyboard, timer, VGA, ATA, block-cache, PIC, and port-I/O notes from the original driver guide.


## keyboard (ps/2)

file: `src/kernel/drivers/keyboard.c`

### inisialisasi

```c
void keyboard_init(void) {
 (void)inb(KEYBOARD_DATA); // buang byte pending
 ctrl_pressed = 0;
 extended_key = 0;
}
```

### handler interrupt (irq1)

```c
void keyboard_interrupt_handler(void) {
 uint8_t scancode = inb(KEYBOARD_DATA); // baca dari port 0x60

 if (scancode == 0xE0) { extended_key = 1; return; }
 if (extended_key) {
 // arrow keys, delete, home, end, pgup, pgdn
 extended_key = 0;
 if (scancode & 0x80) return; // release
 uint8_t key = ...; // konversi
 keyboard_buffer[write_pos] = key;
 write_pos = (write_pos + 1) % KEYBOARD_BUFFER_SIZE;
 return;
 }

 // shift
 if (scancode == 0x2A) { shift_pressed = 1; return; }
 if (scancode == 0xAA) { shift_pressed = 0; return; }
 // ctrl
 if (scancode == 0x1D) { ctrl_pressed = 1; return; }
 if (scancode == 0x9D) { ctrl_pressed = 0; return; }

 if (scancode & 0x80) return; // tombol release, cuekin

 const char *map = shift_pressed ? keymap_shift : keymap_us;
 char c = map[scancode];
 if (c != 0) {
 keyboard_buffer[write_pos] = c;
 write_pos = (write_pos + 1) % KEYBOARD_BUFFER_SIZE;
 }
}
```

### circular buffer

```c
static uint8_t keyboard_buffer[KEYBOARD_BUFFER_SIZE]; // 
static volatile int read_pos = 0;
static volatile int write_pos = 0;
```

### keyboard_getchar

```c
char keyboard_getchar(void) {
 while (read_pos == write_pos) {
 asm volatile("sti; hlt"); // tunggu interrupt
 }
 uint8_t key = keyboard_buffer[read_pos];
 read_pos = (read_pos + 1) % KEYBOARD_BUFFER_SIZE;
 return (char)key;
}
```

`sti; hlt` adalah atomik di x86: interrupt di-enable saat cpu hlt, jadi kalau ada keyboard irq, cpu akan bangun.

### keymap

```c
static const char keymap_us[128] = {
 0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
 '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
};
```

## timer (pit)

file: `src/kernel/drivers/timer.c`

### inisialisasi

```c
void timer_init(uint32_t frequency) {
 uint32_t divisor = PIT_FREQUENCY / frequency; // 1193182 / 100 = 11931

 outb(PIT_CONTROL, 0x36); // channel 0, mode 2 (rate generator)
 outb(PIT_CHANNEL_0, divisor & 0xFF); // low byte
 outb(PIT_CHANNEL_0, (divisor >> 8) & 0xFF); // high byte

 pic_enable_irq(0); // enable timer irq di pic
}
```

### handler

```c
void timer_interrupt_handler(void) {
 ticks++;
 task_switch();
}
```

100hz -> tiap 10ms trigger sekali. `ticks` counter bisa dipakai untuk uptime.

### timer_get_ticks / timer_sleep

```c
uint32_t timer_get_ticks(void) { return ticks; }

void timer_sleep(uint32_t ms) {
 uint32_t target = ticks + (ms / 10);
 while (ticks < target);
}
```

## vga text mode

file: `src/kernel/core/vga.c`

### memory

```c
#define VGA_MEMORY 0xB8000
static uint16_t* vga_buffer = (uint16_t*)VGA_MEMORY;
```

tiap karakter : char (low) + attribute (high).
attribute: 4-bit foreground + 4-bit background.

### fungsi

```c
void vga_putc(char c); // tulis karakter, handle \n, \b, scroll
void vga_print(const char* s); // print string
void vga_clear(void); // bersihin layar
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_set_cursor(uint8_t x, uint8_t y);
void vga_write_char(uint8_t x, uint8_t y, char c, uint8_t color);
void vga_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, char c, uint8_t color);
```

### vga_putc

```c
void vga_putc(char c) {
 if (c == '\n') { cursor_x = 0; cursor_y++; if (cursor_y >= 25) scroll(); return; }
 if (c == '\b') { if (cursor_x > 0) cursor_x--; buffer[...] = ' '; return; }

 vga_buffer[cursor_y * 80 + cursor_x] = c | (color << 8);
 cursor_x++;
 if (cursor_x >= 80) { cursor_x = 0; cursor_y++; if (cursor_y >= 25) scroll(); }
}
```

### scroll

geser layar ke atas banyak baris:
```c
for (y = 1; y < 25; y++)
 for (x = 0; x < 80; x++)
 buffer[(y-1)*80 + x] = buffer[y*80 + x];
// clear baris terakhir
for (x = 0; x < 80; x++)
 buffer[24*80 + x] = ' ' | (color << 8);
```

### hardware cursor

program cursor position via crtc registers:
```c
uint16_t pos = y * 80 + x;
outb(VGA_CTRL_REG, 0x0F); outb(VGA_DATA_REG, pos & 0xFF);
outb(VGA_CTRL_REG, 0x0E); outb(VGA_DATA_REG, (pos >> 8) & 0xFF);
```

## ata (ide) -- pio mode

file: `src/kernel/drivers/ata.c`

### port

```text
0x1F0: data port (16-bit)
0x1F2: sector count
0x1F3: lba low
0x1F4: lba mid
0x1F5: lba high
0x1F6: drive/head (0xE0 = master, 0xF0 = slave)
0x1F7: command/status
```

### read sector (pio, lba28)

```c
ata_read_sector(uint32_t lba, void *buffer) {
 // pilih drive
 outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
 // set lba
 outb(0x1F2, 1); // sector count
 outb(0x1F3, lba & 0xFF);
 outb(0x1F4, (lba >> 8) & 0xFF);
 outb(0x1F5, (lba >> 16) & 0xFF);
 // command
 outb(0x1F7, 0x20); // read sectors with retry

 // polling status
 while (inb(0x1F7) & 0x80); // wait not busy
 while (!(inb(0x1F7) & 0x08)); // wait drq

 // read data (256 word = )
 for (int i = 0; i < 256; i++)
 ((uint16_t*)buffer)[i] = inw(0x1F0);
}
```

## block cache

file: `src/kernel/drivers/block.c`

entry cache untuk mengurangi akses disk langsung.

```c
typedef struct {
 uint32_t block_num;
 uint8_t data[BLOCK_SIZE]; // 
 int valid;
 int dirty;
} cache_entry_t;

static cache_entry_t cache[64];
```

### block_read

```c
int block_read(uint32_t abs_block, uint8_t *buffer) {
 // cek cache
 for (int i = 0; i < 64; i++) {
 if (cache[i].valid && cache[i].block_num == abs_block) {
 memcpy(buffer, cache[i].data, BLOCK_SIZE);
 return 0;
 }
 }
 // cache miss -> baca dari disk via ata
 ata_read_sector(abs_block, buffer);
 // simpen ke cache (evict kalo penuh)
 int slot = find_free_cache();
 cache[slot].block_num = abs_block;
 memcpy(cache[slot].data, buffer, BLOCK_SIZE);
 cache[slot].valid = 1;
 cache[slot].dirty = 0;
 return 0;
}
```

### block_write

```c
int block_write(uint32_t abs_block, const uint8_t *buffer) {
 // cari slot cache
 int slot = find_free_cache();
 cache[slot].block_num = abs_block;
 memcpy(cache[slot].data, buffer, BLOCK_SIZE);
 cache[slot].valid = 1;
 cache[slot].dirty = 1; // marked dirty, bakal di-flush nanti
 // langsung tulis ke disk juga
 ata_write_sector(abs_block, buffer);
 cache[slot].dirty = 0;
 return 0;
}
```

### block_flush

tulis semua dirty cache ke disk:
```c
for (int i = 0; i < 64; i++)
 if (cache[i].valid && cache[i].dirty)
 ata_write_sector(cache[i].block_num, cache[i].data);
```

## pic

file: `src/kernel/drivers/pic.c`

### init

remap irq 0-7 ke interrupt 32-39, irq 8-15 ke 40-47:
```c
outb(PIC_MASTER_CMD, ICW1_INIT | ICW1_ICW4); // 0x11
outb(PIC_SLAVE_CMD, ICW1_INIT | ICW1_ICW4);
outb(PIC_MASTER_DATA, 32); // icw2: master base 32
outb(PIC_SLAVE_DATA, 40); // icw2: slave base 40
outb(PIC_MASTER_DATA, 0x04); // icw3: slave on irq2
outb(PIC_SLAVE_DATA, 0x02); // icw3: slave id 2
outb(PIC_MASTER_DATA, ICW4_8086); // 0x01
outb(PIC_SLAVE_DATA, ICW4_8086);
outb(PIC_MASTER_DATA, 0xFF); // mask semua irq dulu
outb(PIC_SLAVE_DATA, 0xFF);
```

### enable/disable irq

```c
void pic_enable_irq(int irq) {
 uint16_t port = (irq < 8) ? PIC_MASTER_DATA : PIC_SLAVE_DATA;
 if (irq >= 8) irq -= 8;
 uint8_t mask = inb(port);
 mask &= ~(1 << irq); // clear bit = enable
 outb(port, mask);
}
```

## io ports

file: `src/kernel/drivers/io.c`

```c
void outb(uint16_t port, uint8_t val) {
 asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
uint8_t inb(uint16_t port) {
 uint8_t ret;
 asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
 return ret;
}
```
