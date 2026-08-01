# I/o port

this page explains bagaimana OaSis komunikasi same hardware lewat port I/O.

## Contents

- [overview](#overview)
- [port i/o vs memory mapped i/o](#port-io-vs-memory-mapped-io)
- [I/O instructions](#instructions-io)
- [api reference](#api-reference)

---

## Overview

**port I/O** is cara cpu komunikasi same peripheral devices lewat dedicated I/O ports.

### Karakteristik

- **separate address space**: I/O ports punya address space sendiri (64 KB)
- **special instructions**: use `in` and `out` instructions
- **legacy**: used by many legacy devices (VGA, keyboard, PIT, and other-other)

### Why still used?

- hardware legacy still use port I/O
- simpler for some devices
- backward compatibility

## Port i/o vs memory mapped i/o

### Port i/o

```text
cpu ←→ I/O port space (64 KB) ←→ device
```

**instructions:**
```asm
in al, 0x60      ; read from port 0x60
out 0x60, al     ; write to port 0x60
```

### Memory mapped i/o (MMIO)

```text
cpu ←→ memory space ←→ device
```

**instructions:**
```asm
mov al, [0xB8000]  ; read from the memory-mapped device
mov [0xB8000], al  ; write to the memory-mapped device
```

### Perbandingan

| aspek | port i/o | MMIO |
|-------|----------|------|
| address space | separate (64 KB) | shared with memory |
| instructions | special (in/out) | normal memory access |
| speed | slower | faster |
| usage | legacy devices | modern devices |

## I/O instructions

x86 has two instructions for port I/O:

### In (read from port)

```asm
; read 1 byte
in al, port      ; al = inb(port)

; read 2 bytes (word)
in ax, port      ; ax = inw(port)

; read 4 bytes (dword)
in eax, port     ; eax = inl(port)
```

### Out (write to the port)

```asm
; write 1 byte
out port, al     ; outb(port, al)

; write 2 bytes (word)
out port, ax     ; outw(port, ax)

; write 4 bytes (dword)
out port, eax    ; outl(port, eax)
```

## Api reference

OaSis provide helper functions for port I/O:

### Read byte

```c
uint8_t inb(uint16_t port);
```

read 1 byte from port.

**parameter:**
- `port`: port address (0-65535)

**return:** byte that dibaca

**implementasi:**
```c
uint8_t inb(uint16_t port) {
    uint8_t result;
    asm volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}
```

### Write byte

```c
void outb(uint16_t port, uint8_t value);
```

write 1 byte to the port.

**parameter:**
- `port`: port address
- `value`: byte that mau ditulis

**implementasi:**
```c
void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}
```

### Read word

```c
uint16_t inw(uint16_t port);
```

read 2 bytes (word) from port.

**parameter:**
- `port`: port address

**return:** word that dibaca

**implementasi:**
```c
uint16_t inw(uint16_t port) {
    uint16_t result;
    asm volatile("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}
```

### Write word

```c
void outw(uint16_t port, uint16_t value);
```

write 2 bytes (word) to the port.

**parameter:**
- `port`: port address
- `value`: word that mau ditulis

**implementasi:**
```c
void outw(uint16_t port, uint16_t value) {
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}
```

### Read dword

```c
uint32_t inl(uint16_t port);
```

read 4 bytes (dword) from port.

**parameter:**
- `port`: port address

**return:** dword that dibaca

### Write dword

```c
void outl(uint16_t port, uint32_t value);
```

write 4 bytes (dword) to the port.

**parameter:**
- `port`: port address
- `value`: dword that mau ditulis

### I/o delay

```c
void io_wait(void);
```

delay sebentar after I/O operation.

**implementasi:**
```c
void io_wait(void) {
    asm volatile("outb %%al, $0x80" : : "a"(0));
}
```

**why butuh delay?**
- beberapa device lambat
- perlu waktu for process command
- prevent race condition

---

## Usage example

### Read keyboard scancode

```c
uint8_t scancode = inb(0x60);
```

### Write to VGA

```c
// the VGA controller does not use port I/O; it uses MMIO
// this is an example for another device
outb(0x3C4, 0x02);  // select register
outb(0x3C5, 0x0F);  // write data
```

### Read from disk

```c
uint16_t data = inw(0x1F0);  // read one word from the ATA data port
```

---

## Common ports

common OaSis ports:

### Keyboard (0x60-0x64)

| port | deskripsi |
|------|-----------|
| 0x60 | data port (scancode) |
| 0x64 | status port |

### Pit timer (0x40-0x43)

| port | deskripsi |
|------|-----------|
| 0x40 | channel 0 data |
| 0x43 | command register |

### Pic (0x20-0x21, 0xA0-0xA1)

| port | deskripsi |
|------|-----------|
| 0x20 | master pic command |
| 0x21 | master pic data |
| 0xA0 | slave pic command |
| 0xA1 | slave pic data |

### Ata/ide (0x1F0-0x1F7)

| port | deskripsi |
|------|-----------|
| 0x1F0 | data port (16-bit) |
| 0x1F2 | sector count |
| 0x1F3 | LBA low |
| 0x1F4 | LBA mid |
| 0x1F5 | LBA high |
| 0x1F6 | drive/head |
| 0x1F7 | status/command |

### Cmos/rtc (0x70-0x71)

| port | deskripsi |
|------|-----------|
| 0x70 | address register |
| 0x71 | data register |

---

## Troubleshooting

### Port not response

- cek port address bener
- cek device enabled
- cek I/O permission (if ada)

### Data corrupt

- cek data size (byte/word/dword)
- check timing (may need io_wait)
- cek device ready

### System hang

- check for an infinite loop in the wait function
- cek device status flags
- cek timeout handling

---

**back to:** [driver →](readme.md)
