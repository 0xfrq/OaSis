# Day 2 – Oasis OS: Building a Stable Text Console (VGA + Keyboard)

## Goal of Day 2

The goal of Day 2 is to turn a "running kernel" into a **usable interactive system**.

- On Day 1, we proved that the kernel executes
- On Day 2, we prove that the kernel can interact with the user reliably

By the end of Day 2, we must be able to say with confidence:

- The kernel fully controls the screen (VGA text mode)
- BIOS no longer interferes with screen output
- Text output is clean, stable, and predictable
- The kernel can read keyboard input
- Each key press produces exactly one character
- Backspace behaves correctly
- The system does not flicker, spam, or overwrite itself

This is the foundation of every CLI-based operating system.

---

## Conceptual Overview

Day 2 introduces two critical subsystems:

- **VGA Text Driver** – controls how text appears on screen
- **Keyboard Driver (Polling)** – reads user input directly from hardware

At this stage:

- We do not use interrupts
- We do not use BIOS services
- We interact with hardware directly

This keeps the system simple, deterministic, and debuggable.

---

## VGA Text Mode Fundamentals

In x86 text mode:

- VGA memory starts at address `0xB8000`
- Screen size is 80 columns × 25 rows
- Each screen cell uses 2 bytes:
  - Byte 0: ASCII character
  - Byte 1: Color attribute

Memory layout (conceptually):

```
[ char ][ color ][ char ][ color ] ...
```

Writing to this memory immediately updates the screen.

There is:

- No buffering
- No safety
- No abstraction

The kernel must manage everything itself.

---

## VGA Driver Design

Instead of writing to `0xB8000` everywhere, we introduce a VGA driver.

Responsibilities of the VGA driver:

- Clear the screen
- Track cursor position (x, y)
- Print a single character safely
- Handle special characters (`\n`, `\b`)
- Print strings using the character printer

This centralizes all screen logic and prevents memory corruption.

---

## VGA Driver Interface (kernel/vga.h)

```c
#ifndef VGA_H
#define VGA_H

#include <stdint.h>

void vga_clear(void);
void vga_putc(char c);
void vga_print(const char* str);

#endif
```

---

## VGA Driver Implementation (kernel/vga.c)

```c
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
        if (cursor_y < VGA_HEIGHT - 1)
            cursor_y++;
        return;
    }

    if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
            vga_buffer[cursor_y * VGA_WIDTH + cursor_x] =
                vga_entry(' ', color);
        }
        return;
    }

    if (cursor_y >= VGA_HEIGHT)
        return;

    vga_buffer[cursor_y * VGA_WIDTH + cursor_x] =
        vga_entry(c, color);

    cursor_x++;

    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        if (cursor_y < VGA_HEIGHT - 1)
            cursor_y++;
    }
}

void vga_print(const char* str) {
    for (int i = 0; str[i]; i++) {
        vga_putc(str[i]);
    }
}
```

---

## Disabling BIOS Interference

Even after booting into our kernel, BIOS interrupt handlers are still active unless explicitly disabled.

BIOS can:

- Handle keyboard input
- Update the screen
- Trigger timer-based behavior

This causes:

- Flickering text
- Disappearing output
- Unpredictable behavior

To prevent this, we mask all PIC interrupts early in `kernel_main()`.

---

## Port Output Helper (kernel/io.h)

```c
#ifndef IO_H
#define IO_H

#include <stdint.h>

void outb(uint16_t port, uint8_t value);

#endif
```

---

## Port Output Implementation (kernel/io.c)

```c
#include "io.h"

void outb(uint16_t port, uint8_t value) {
    __asm__ __volatile__ (
        "outb %0, %1"
        :
        : "a"(value), "Nd"(port)
    );
}
```

Masking the PIC:

```c
outb(0x21, 0xFF);
outb(0xA1, 0xFF);
```

After this:

- BIOS is effectively disabled
- The kernel fully owns the machine

---

## Keyboard Input Fundamentals (Polling)

We use polling, not interrupts.

Important ports:

- `0x64` – Keyboard controller status
- `0x60` – Keyboard data (scancode)

Correct process:

1. Wait until `(inb(0x64) & 1)` is set
2. Read scancode from `0x60`
3. Ignore key release scancodes (`scancode & 0x80`)
4. Translate scancode using a keymap

---

## Keyboard Driver Interface (kernel/keyboard.h)

```c
#ifndef KEYBOARD_H
#define KEYBOARD_H

char keyboard_getchar(void);

#endif
```

---

## Keyboard Driver (Final, Correct – kernel/keyboard.c)

```c
#include "keyboard.h"
#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ __volatile__ (
        "inb %1, %0"
        : "=a"(ret)
        : "dN"(port)
    );
    return ret;
}

static const char keymap[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0,'\\',
    'z','x','c','v','b','n','m',',','.','/', 0,'*', 0,' ',
};

char keyboard_getchar(void) {
    uint8_t scancode;

    /* wait for key press */
    while (1) {
        if (!(inb(0x64) & 1))
            continue;

        scancode = inb(0x60);

        if (scancode & 0x80)
            continue;

        break;
    }

    /* wait for key release */
    while (1) {
        if (!(inb(0x64) & 1))
            continue;

        if (inb(0x60) & 0x80)
            break;
    }

    if (scancode == 0x0E)
        return '\b';

    return keymap[scancode];
}
```

This guarantees:

- No key spamming
- One character per press
- Correct backspace behavior

---

## Kernel Main Loop (kernel/kernel.c)

```c
#include "vga.h"
#include "keyboard.h"
#include "io.h"

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
```

---

## End of Day 2 – What We Achieved

At the end of Day 2:

- ✓ Screen output is clean and stable
- ✓ BIOS no longer interferes
- ✓ Cursor is tracked correctly
- ✓ Keyboard input works reliably
- ✓ Backspace behaves correctly
- ✓ No flickering or random characters
- ✓ System is interactive

**Oasis now has a real console.**