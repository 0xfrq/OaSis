# Day 3 – Oasis OS: Building a Real CLI (Input Buffer, Commands, Scrolling)

## Goal of Day 3

The goal of Day 3 is to transform a raw interactive console into a **real command-line interface (CLI)**.

On Day 2, Oasis could:

- Print text
- Read keyboard input
- Handle backspace
- Remain stable

However, input was character-by-character, not line-based.

By the end of Day 3, we must be able to say:

- The kernel supports line-based input
- The Enter key submits a command
- Input is stored in a buffer
- Commands are parsed and executed
- The screen scrolls when full
- A basic shell loop exists

This is the moment Oasis becomes a real operating system shell.

---

## Conceptual Shift (Very Important)

**Before Day 3:**

- Input = characters
- Output = characters
- No structure

**From Day 3 onward:**

- Input = lines
- Output = responses
- Execution = commands

This is the birth of the shell.

---

## Step 1 – Add Screen Scrolling

Without scrolling:

- Cursor reaches bottom
- Output stops
- Screen looks frozen

We implement vertical scrolling:

- When `cursor_y` reaches 25
- Move all rows up by one
- Clear the last row
- Continue printing

---

## Update VGA Driver (kernel/vga.c)

Replace `kernel/vga.c` with this final Day 3 version:

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

static void vga_scroll(void) {
    for (int y = 1; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer[(y - 1) * VGA_WIDTH + x] =
                vga_buffer[y * VGA_WIDTH + x];
        }
    }

    for (int x = 0; x < VGA_WIDTH; x++) {
        vga_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] =
            vga_entry(' ', color);
    }

    cursor_y = VGA_HEIGHT - 1;
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
            vga_buffer[cursor_y * VGA_WIDTH + cursor_x] =
                vga_entry(' ', color);
        }
        return;
    }

    vga_buffer[cursor_y * VGA_WIDTH + cursor_x] =
        vga_entry(c, color);

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
```

---

## Step 2 – Line Input Buffer

We now store input into a buffer until Enter is pressed.

Design:

- Fixed-size buffer (simple, safe)
- Backspace edits the buffer
- Enter submits the line

---

## Step 3 – Add Simple String Utilities

We need minimal string helpers (no libc).

Create files:

```bash
touch kernel/string.h kernel/string.c
```

---

## kernel/string.h

```c
#ifndef STRING_H
#define STRING_H

int strcmp(const char* a, const char* b);

#endif
```

---

## kernel/string.c

```c
#include "string.h"

int strcmp(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i])
            return a[i] - b[i];
        i++;
    }
    return a[i] - b[i];
}
```

---

## Step 4 – Command Execution Logic

We support a minimal command set:

- `help`
- `clear`

---

## Step 5 – Kernel Shell Loop

### Update kernel/kernel.c

Replace with:

```c
#include "vga.h"
#include "keyboard.h"
#include "io.h"
#include "string.h"

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

        if (c == '\n') {
            input[index] = 0;
            vga_putc('\n');

            if (strcmp(input, "help") == 0) {
                vga_print("commands:\n");
                vga_print("  help  - show this message\n");
                vga_print("  clear - clear screen\n");
            } else if (strcmp(input, "clear") == 0) {
                vga_clear();
            } else if (index != 0) {
                vga_print("unknown command\n");
            }

            index = 0;
            vga_print("> ");
            continue;
        }

        if (c == '\b') {
            if (index > 0) {
                index--;
                vga_putc('\b');
            }
            continue;
        }

        if (index < INPUT_MAX - 1) {
            input[index++] = c;
            vga_putc(c);
        }
    }
}
```

---

## Step 6 – Update Makefile

Add `string.c`.

Modify kernel rule:

```makefile
kernel.bin:
	nasm -f elf32 boot/entry.asm -o entry.o
	$(CC) $(CFLAGS) -c kernel/kernel.c -o kernel.o
	$(CC) $(CFLAGS) -c kernel/vga.c -o vga.o
	$(CC) $(CFLAGS) -c kernel/keyboard.c -o keyboard.o
	$(CC) $(CFLAGS) -c kernel/io.c -o io.o
	$(CC) $(CFLAGS) -c kernel/string.c -o string.o
	$(LD) $(LDFLAGS) -T kernel/linker.ld -o kernel.bin \
	    entry.o kernel.o vga.o keyboard.o io.o string.o
```

---

## Step 7 – Build and Run

### Build

```bash
make clean
make
```

### Run (development mode)

```bash
qemu-system-i386 -kernel kernel.bin -m 128M
```

---

## Expected Result

On boot:

```
OASIS kernel booted successfully
type 'help' for commands

> 
```

Typing:

```
> help
commands:
  help  - show this message
  clear - clear screen
```

Typing:

```
> clear
```

Clears the screen and shows a new prompt.

---

## End of Day 3 – What We Achieved

- ✓ Line-based input
- ✓ Enter key handling
- ✓ Backspace editing
- ✓ Screen scrolling
- ✓ Command parsing
- ✓ Real shell loop

**Oasis now has a functional CLI.**

---

## What Comes Next (Day 4)

Day 4 moves into core OS infrastructure:

- Timer (PIT)
- Delays and uptime
- Interrupt Descriptor Table (IDT)
- Hardware interrupts
- Replacing polling with IRQs

From here on, Oasis stops being "simple" and starts being a real operating system kernel.