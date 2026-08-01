---
layout: default
title: driver
---

# Drivers

This section documents the hardware boundary of OaSis. The [networking internals](networking/) page covers the complete PCI-to-ICMP path; the pages below cover the earlier device drivers.

## Driver catalog

| driver | status | Source |
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

read [Networking internals](networking/) for PCI discovery, RTL8139 DMA, Ethernet frames, ARP resolution, IPv4 validation, ICMP echo, QEMU setup, and troubleshooting.

## Legacy driver details

The sections below retain the detailed keyboard, timer, VGA, ATA, block-cache, PIC, and port-I/O notes from the original driver guide.


## Keyboard (ps/2)

file: `src/kernel/drivers/keyboard.c`

### Initialization

```c
void keyboard_init(void) {
 (void)inb(KEYBOARD_DATA); // buang byte pending
 ctrl_pressed = 0;
 extended_key = 0;
}
```

### Handler interrupt (irq1)

```c
void keyboard_interrupt_handler(void) {
 uint8_t scancode = inb(KEYBOARD_DATA); // read from port 0x60

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

### Circular buffer

```c
static uint8_t keyboard_buffer[KEYBOARD_BUFFER_SIZE]; // 
static volatile int read_pos = 0;
static volatile int write_pos = 0;
```

### Keyboard_getchar

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

`sti; hlt` is atomic on x86: interrupts are enabled when the CPU halts, so a keyboard IRQ wakes the CPU.

### Keymap

```c
static const char keymap_us[128] = {
 0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
 '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
};
```

## Timer (pit)

file: `src/kernel/drivers/timer.c`

### Initialization

```c
void timer_init(uint32_t frequency) {
 uint32_t divisor = PIT_FREQUENCY / frequency; // 1193182 / 100 = 11931

 outb(PIT_CONTROL, 0x36); // channel 0, mode 2 (rate generator)
 outb(PIT_CHANNEL_0, divisor & 0xFF); // low byte
 outb(PIT_CHANNEL_0, (divisor >> 8) & 0xFF); // high byte

 pic_enable_irq(0); // enable the timer IRQ in the PIC
}
```

### Handler

```c
void timer_interrupt_handler(void) {
 ticks++;
 task_switch();
}
```

100hz -> fires every 10 ms. `ticks` counter can used for uptime.

### Timer_get_ticks / timer_sleep

```c
uint32_t timer_get_ticks(void) { return ticks; }

void timer_sleep(uint32_t ms) {
 uint32_t target = ticks + (ms / 10);
 while (ticks < target);
}
```

## Vga text mode

file: `src/kernel/core/vga.c`

### Memory

```c
# Define VGA_MEMORY 0xB8000
static uint16_t* vga_buffer = (uint16_t*)VGA_MEMORY;
```

each character : char (low) + attribute (high).
attribute: 4-bit foreground + 4-bit background.

### Function

```c
void vga_putc(char c); // write a character, handle \n, \b, scroll
void vga_print(const char* s); // print string
void vga_clear(void); // bersihin layar
void vga_set_color(uint8_t fg, uint8_t bg);
void vga_set_cursor(uint8_t x, uint8_t y);
void vga_write_char(uint8_t x, uint8_t y, char c, uint8_t color);
void vga_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, char c, uint8_t color);
```

### Vga_putc

```c
void vga_putc(char c) {
 if (c == '\n') { cursor_x = 0; cursor_y++; if (cursor_y >= 25) scroll(); return; }
 if (c == '\b') { if (cursor_x > 0) cursor_x--; buffer[...] = ' '; return; }

 vga_buffer[cursor_y * 80 + cursor_x] = c | (color << 8);
 cursor_x++;
 if (cursor_x >= 80) { cursor_x = 0; cursor_y++; if (cursor_y >= 25) scroll(); }
}
```

### Scroll

move the screen up by several lines:
```c
for (y = 1; y < 25; y++)
 for (x = 0; x < 80; x++)
 buffer[(y-1)*80 + x] = buffer[y*80 + x];
// clear last row
for (x = 0; x < 80; x++)
 buffer[24*80 + x] = ' ' | (color << 8);
```

### Hardware cursor

program cursor position via crtc registers:
```c
uint16_t pos = y * 80 + x;
outb(VGA_CTRL_REG, 0x0F); outb(VGA_DATA_REG, pos & 0xFF);
outb(VGA_CTRL_REG, 0x0E); outb(VGA_DATA_REG, (pos >> 8) & 0xFF);
```

## Ata (ide) -- pio mode

file: `src/kernel/drivers/ata.c`

### Port

```text
0x1F0: data port (16-bit)
0x1F2: sector count
0x1F3: lba low
0x1F4: lba mid
0x1F5: lba high
0x1F6: drive/head (0xE0 = master, 0xF0 = slave)
0x1F7: command/status
```

### Read sector (pio, lba28)

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

## Block cache

file: `src/kernel/drivers/block.c`

entry cache for mengurangi akses disk directly.

```c
typedef struct {
 uint32_t block_num;
 uint8_t data[BLOCK_SIZE]; // 
 int valid;
 int dirty;
} cache_entry_t;

static cache_entry_t cache[64];
```

### Block_read

```c
int block_read(uint32_t abs_block, uint8_t *buffer) {
 // cek cache
 for (int i = 0; i < 64; i++) {
 if (cache[i].valid && cache[i].block_num == abs_block) {
 memcpy(buffer, cache[i].data, BLOCK_SIZE);
 return 0;
 }
 }
 // cache miss -> read from disk through ATA
 ata_read_sector(abs_block, buffer);
 // store in the cache (evict if full)
 int slot = find_free_cache();
 cache[slot].block_num = abs_block;
 memcpy(cache[slot].data, buffer, BLOCK_SIZE);
 cache[slot].valid = 1;
 cache[slot].dirty = 0;
 return 0;
}
```

### Block_write

```c
int block_write(uint32_t abs_block, const uint8_t *buffer) {
 // find a free cache slot
 int slot = find_free_cache();
 cache[slot].block_num = abs_block;
 memcpy(cache[slot].data, buffer, BLOCK_SIZE);
 cache[slot].valid = 1;
 cache[slot].dirty = 1; // marked dirty, bakal di-flush nanti
 // write directly to disk as well
 ata_write_sector(abs_block, buffer);
 cache[slot].dirty = 0;
 return 0;
}
```

### Block_flush

write all dirty cache entries to disk:
```c
for (int i = 0; i < 64; i++)
 if (cache[i].valid && cache[i].dirty)
 ata_write_sector(cache[i].block_num, cache[i].data);
```

## Pic

file: `src/kernel/drivers/pic.c`

### Init

remap IRQ 0-7 to interrupts 32-39, irq 8-15 to 40-47:
```c
outb(PIC_MASTER_CMD, ICW1_INIT | ICW1_ICW4); // 0x11
outb(PIC_SLAVE_CMD, ICW1_INIT | ICW1_ICW4);
outb(PIC_MASTER_DATA, 32); // icw2: master base 32
outb(PIC_SLAVE_DATA, 40); // icw2: slave base 40
outb(PIC_MASTER_DATA, 0x04); // icw3: slave on irq2
outb(PIC_SLAVE_DATA, 0x02); // icw3: slave id 2
outb(PIC_MASTER_DATA, ICW4_8086); // 0x01
outb(PIC_SLAVE_DATA, ICW4_8086);
outb(PIC_MASTER_DATA, 0xFF); // mask all IRQs first
outb(PIC_SLAVE_DATA, 0xFF);
```

### Enable/disable irq

```c
void pic_enable_irq(int irq) {
 uint16_t port = (irq < 8) ? PIC_MASTER_DATA : PIC_SLAVE_DATA;
 if (irq >= 8) irq -= 8;
 uint8_t mask = inb(port);
 mask &= ~(1 << irq); // clear bit = enable
 outb(port, mask);
}
```

## Io ports

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
