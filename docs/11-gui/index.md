---
layout: default
title: GUI roadmap
description: track the planned path from VGA text mode to a graphical OaSis desktop.
content_type: roadmap
audience: contributors and operating-system learners
---

# GUI roadmap

OaSis is currently a VGA text-mode operating system. This page describes future work; no framebuffer driver, mouse driver, compositor, or GUI syscall exists in the current tree.

## Current display

`src/kernel/core/vga.c` writes character and color cells to the VGA text buffer at `0xB8000`. The shell, editor, diagnostics, compiler output, and network commands all use this interface.

The current display provides:

- 80 columns by 25 rows.
- 16 foreground colors and 16 background colors.
- Scrolling, backspace handling, and a hardware cursor.
- simple rectangular character fills used by the boot screen and editor.

It does not provide pixel drawing, a framebuffer abstraction, windows, mouse input, or user-space graphics.

## Planned milestones

### 1. Framebuffer discovery

read the Multiboot framebuffer information when available. Add a kernel framebuffer API that respects the physical address, pitch, width, height, and bytes per pixel instead of assuming a fixed VGA address.

Proposed primitives include:

```c
fb_clear(color);
fb_put_pixel(x, y, color);
fb_fill_rect(x, y, width, height, color);
fb_draw_rect(x, y, width, height, color);
```

Acceptance criterion: QEMU can boot to a known pixel pattern without breaking the existing text-mode fallback.

### 2. Bitmap font and graphical terminal

Add a small bitmap font and render the existing shell into a framebuffer-backed terminal. Keep keyboard input and command behavior unchanged while replacing character-cell output with a graphical surface.

Acceptance criterion: `help`, filesystem commands, `occ`, and `ping` remain usable in a graphical terminal.

### 3. Mouse input

Add PS/2 mouse initialization, IRQ 12 handling, three-byte packet decoding, cursor bounds checking, and button state. Store events in a queue that can be consumed by the compositor.

Acceptance criterion: the cursor moves without corrupting terminal output and button transitions are observable by the GUI layer.

### 4. Compositor and windows

Introduce a kernel-owned compositor with a background, top bar, terminal window, and editor window. Start with fixed surfaces and clipping before implementing arbitrary window movement or resizing.

Acceptance criterion: overlapping surfaces redraw in a deterministic order and keyboard focus is visible.

### 5. Event routing and GUI syscalls

Define process-owned event queues and carefully validate user pointers before exposing GUI operations through `int 0x80`. Add APIs for creating a surface, drawing primitives, receiving input, and presenting a buffer.

Acceptance criterion: a ring 3 program can draw into an isolated surface without writing directly to kernel memory.

### 6. User-space GUI applications

Build a small user-space GUI library for `occ` programs. Port the editor first, then add simple diagnostic applications such as a task monitor or network status panel.

## Design constraints

The GUI should preserve the current shell as a recovery path. It must not assume that every boot has a framebuffer, and it must not grant user programs unrestricted access to physical video memory.

The first implementation should prioritize a reliable framebuffer console over desktop features. Networking, filesystem, and user-mode behavior should remain testable even when GUI initialization fails.
