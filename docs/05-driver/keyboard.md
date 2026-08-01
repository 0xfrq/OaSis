---
layout: default
title: Keyboard driver
description: Explain PS/2 keyboard input, scancodes, and the keyboard buffer.
content_type: reference
audience: operating-system learners and contributors
---

# Keyboard driver

The PS/2 keyboard driver reads scancodes from port `0x60`, converts them to characters, and stores them in a circular buffer for the shell and editor.

## Capabilities

- US QWERTY keymap.
- Shift and Ctrl modifiers.
- Arrow, Home, End, Page Up, Page Down, and Delete keys.
- Blocking and non-blocking character access.

USB keyboards, Caps Lock state, function keys, Alt, and keyboard LEDs are not implemented.

## Ports and scancodes

The keyboard uses two I/O ports:

| Port | Purpose |
| --- | --- |
| `0x60` | Keyboard data and scancodes. |
| `0x64` | Controller status. |

A make code means that a key was pressed. A break code means that it was released. Break codes have bit 7 set. Extended keys begin with prefix `0xE0`.

```c
uint8_t scancode = inb(0x60);
```

## Key maps

The driver has one table for normal keys and one table for Shift. It converts the make code to an ASCII character.

```c
static const char keymap_us[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
};
```

## Circular buffer

Keyboard interrupts can arrive before the shell is ready to read them. The circular buffer stores pending keys until the consumer reads them.

```text
IRQ 1
  -> read port 0x60
  -> ignore release codes
  -> convert the make code
  -> write the key to the buffer
  -> send EOI to the PIC
```

`keyboard_available()` checks the buffer without blocking. When it is empty, the shell uses `hlt` until an interrupt wakes the CPU.

## API

| Function | Purpose |
| --- | --- |
| `keyboard_init()` | Clear pending data and reset modifier state. |
| `keyboard_interrupt_handler()` | Process one keyboard scancode. |
| `keyboard_available()` | Return whether a key is waiting. |
| `keyboard_getchar()` | Block until a printable character is available. |
| `keyboard_getkey()` | Block until any key, including special keys, is available. |
| `keyboard_flush()` | Reset the buffer and modifier state. |

## Troubleshooting

If the keyboard does not respond, confirm that IRQ 1 is unmasked in the PIC, the handler is installed in the IDT, and port `0x60` returns data. For strange characters, check the scancode table and modifier state.
