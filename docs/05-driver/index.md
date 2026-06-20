---
layout: default
title: driver
---

# driver

## keyboard (ps/2)

di `src/kernel/drivers/keyboard.c`.

- handler irq1 baca scancode dari port 0x60
- konversi keymap us (tanpa shift dan dengan shift)
- simpan ke circular buffer 256 byte (`read_pos`/`write_pos`)
- handle extended key (0xE0 prefix), shift, ctrl
- `keyboard_getchar()` nunggu sampe ada karakter di buffer: `sti; hlt`

## timer (pit)

di `src/kernel/drivers/timer.c`.

- inisialisasi pit channel 0 mode 2 (rate generator)
- frequency 100hz -> divisor = 1193182 / 100 = 11931
- handler ngelakuin `ticks++` dan panggil `task_switch()`

## vga text mode

di `src/kernel/core/vga.c`.

- mode text 80x25, memory di 0xB8000
- tiap karakter 2 byte: char + attribute (4-bit fg + 4-bit bg)
- scroll, cursor tracking, color support
- `vga_write_char(x, y, c, color)` untuk draw di posisi tertentu

## ata (ide)

di `src/kernel/drivers/ata.c`.

- pio mode, baca/tulis 1 sector (512 byte) via port 0x1F0-0x1F7
- polling status register (busy, drq)

## block cache

di `src/kernel/drivers/block.c`.

- block cache 64 entry (32kb cache)
- `block_read(abs_block, buf)` -> cek cache dulu, kalo miss baru baca dari ata
- `block_write(abs_block, buf)` -> tulis ke cache + flush ke disk
- `block_flush()` -> tulis semua dirty cache ke disk

## pic

di `src/kernel/drivers/pic.c`.

- master pic (port 0x20/0x21) dan slave pic (0xA0/0xA1)
- remap irq 0-7 ke interrupt 32-39, irq 8-15 ke 40-47
- `pic_enable_irq(irq)` -> clear bit di mask
- eoi: `outb(0x20, 0x20)`

## io

di `src/kernel/drivers/io.c`.

- `outb(port, val)` dan `inb(port)` via inline assembly
