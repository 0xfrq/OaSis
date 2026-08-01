---
layout: default
title: VGA text driver
description: Explain the current VGA text console and its boundary before GUI work.
content_type: reference
audience: operating-system learners and contributors
---

# VGA text driver

OaSis currently provides an 80 by 25 VGA text console. The shell, editor, boot screen, diagnostics, compiler, and network commands all use this driver.

## Current UI boundary

The driver writes character cells to the VGA text buffer at `0xB8000`. It does not provide pixels, a framebuffer abstraction, mouse input, windows, a compositor, or GUI syscalls. See the [GUI roadmap](../11-gui/) for future work.

## Text-mode layout

The display contains 80 columns and 25 rows, for 2,000 cells. Coordinates start at `(0, 0)` and end at `(79, 24)`.

```text
+----------------------------------------+
| (0,0)                            (79,0)|
|                                        |
|                                        |
|                                        |
| (0,24)                        (79,24) |
+----------------------------------------+
```

Each cell occupies two bytes:

```text
offset 0: ASCII character
offset 1: color attribute
```

The cell offset is `(y * 80 + x) * 2`.

## Colors

The attribute byte contains a four-bit foreground color and a four-bit background color. The driver supports the standard 16 VGA colors.

```c
uint8_t color = (background << 4) | foreground;
```

For example, white text on black is `0x0F`, and yellow text on blue is `0x1E`.

## Cursor and scrolling

`vga_putc()` handless printable characters, newline, backspace, line wrapping, and scrolling. `vga_refresh_cursor()` updates the hardware cursor through VGA CRTC ports `0x3D4` and `0x3D5`.

The driver scrolls the text buffer when the cursor reaches the last row. It clears the newly exposed bottom row with the current color.

## API reference

The main functions are:

| function | Purpose |
| --- | --- |
| `vga_clear()` | Clear the screen and reset the cursor. |
| `vga_putc(c)` | write one character and update the cursor. |
| `vga_print(text)` | write a null-terminated string. |
| `vga_set_color(fg, bg)` | Set the current foreground and background colors. |
| `vga_write_char(x, y, c, color)` | write one cell at an explicit position. |
| `vga_fill_rect(...)` | Fill a character-cell rectangle. |
| `vga_set_cursor(x, y)` | Move the hardware cursor. |
| `vga_refresh_cursor()` | Synchronize the hardware cursor. |

## GUI migration plan

The text console remains the recovery path while graphics are developed. The planned stages are:

1. read and map a Multiboot framebuffer.
2. Add pixel and rectangle primitives.
3. Render a bitmap font and graphical terminal.
4. Add PS/2 mouse packets and input events.
5. Add a compositor with terminal and editor windows.
6. Expose validated drawing and input APIs to ring 3 programs.

No GUI source files or symbols are currently implemented.
