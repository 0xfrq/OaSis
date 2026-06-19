# i/o port

dokumentasi ini ngebahas gimana OaSis komunikasi sama hardware lewat port I/O.

## daftar isi

- [overview](#overview)
- [port i/o vs memory mapped i/o](#port-io-vs-memory-mapped-io)
- [instruksi i/o](#instruksi-io)
- [api reference](#api-reference)

---

## overview

**port I/O** adalah cara cpu komunikasi sama peripheral devices lewat dedicated I/O ports.

### karakteristik

- **separate address space**: I/O ports punya address space sendiri (64 KB)
- **special instructions**: pake `in` dan `out` instructions
- **legacy**: banyak dipake di hardware lama (VGA, keyboard, PIT, dll)

### kenapa masih dipake?

- hardware legacy masih pake port I/O
- simpler buat some devices
- backward compatibility

## port i/o vs memory mapped i/o

### port i/o

```
cpu ←→ I/O port space (64 KB) ←→ device
```

**instruksi:**
```asm
in al, 0x60      ; baca dari port 0x60
out 0x60, al     ; tulis ke port 0x60
```

### memory mapped i/o (MMIO)

```
cpu ←→ memory space ←→ device
```

**instruksi:**
```asm
mov al, [0xB8000]  ; baca dari memory-mapped device
mov [0xB8000], al  ; tulis ke memory-mapped device
```

### perbandingan

| aspek | port i/o | MMIO |
|-------|----------|------|
| address space | separate (64 KB) | shared dengan memory |
| instruksi | special (in/out) | normal memory access |
| speed | slower | faster |
| usage | legacy devices | modern devices |

## instruksi i/o

x86 punya 2 instruksi buat port I/O:

### in (baca dari port)

```asm
; baca 1 byte
in al, port      ; al = inb(port)

; baca 2 bytes (word)
in ax, port      ; ax = inw(port)

; baca 4 bytes (dword)
in eax, port     ; eax = inl(port)
```

### out (tulis ke port)

```asm
; tulis 1 byte
out port, al     ; outb(port, al)

; tulis 2 bytes (word)
out port, ax     ; outw(port, ax)

; tulis 4 bytes (dword)
out port, eax    ; outl(port, eax)
```

## api reference

OaSis provide helper functions buat port I/O:

### baca byte

```c
uint8_t inb(uint16_t port);
```

baca 1 byte dari port.

**parameter:**
- `port`: port address (0-65535)

**return:** byte yang dibaca

**implementasi:**
```c
uint8_t inb(uint16_t port) {
    uint8_t result;
    asm volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}
```

### tulis byte

```c
void outb(uint16_t port, uint8_t value);
```

tulis 1 byte ke port.

**parameter:**
- `port`: port address
- `value`: byte yang mau ditulis

**implementasi:**
```c
void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}
```

### baca word

```c
uint16_t inw(uint16_t port);
```

baca 2 bytes (word) dari port.

**parameter:**
- `port`: port address

**return:** word yang dibaca

**implementasi:**
```c
uint16_t inw(uint16_t port) {
    uint16_t result;
    asm volatile("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}
```

### tulis word

```c
void outw(uint16_t port, uint16_t value);
```

tulis 2 bytes (word) ke port.

**parameter:**
- `port`: port address
- `value`: word yang mau ditulis

**implementasi:**
```c
void outw(uint16_t port, uint16_t value) {
    asm volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}
```

### baca dword

```c
uint32_t inl(uint16_t port);
```

baca 4 bytes (dword) dari port.

**parameter:**
- `port`: port address

**return:** dword yang dibaca

### tulis dword

```c
void outl(uint16_t port, uint32_t value);
```

tulis 4 bytes (dword) ke port.

**parameter:**
- `port`: port address
- `value`: dword yang mau ditulis

### i/o delay

```c
void io_wait(void);
```

delay sebentar setelah I/O operation.

**implementasi:**
```c
void io_wait(void) {
    asm volatile("outb %%al, $0x80" : : "a"(0));
}
```

**kenapa butuh delay?**
- beberapa device lambat
- perlu waktu buat process command
- prevent race condition

---

## contoh penggunaan

### baca keyboard scancode

```c
uint8_t scancode = inb(0x60);
```

### tulis ke VGA

```c
// VGA controller gak pake port I/O, pake MMIO
// tapi ini contoh buat device lain
outb(0x3C4, 0x02);  // select register
outb(0x3C5, 0x0F);  // write data
```

### baca dari disk

```c
uint16_t data = inw(0x1F0);  // baca 1 word dari ATA data port
```

---

## common ports

daftar port yang sering dipake di OaSis:

### keyboard (0x60-0x64)

| port | deskripsi |
|------|-----------|
| 0x60 | data port (scancode) |
| 0x64 | status port |

### pit timer (0x40-0x43)

| port | deskripsi |
|------|-----------|
| 0x40 | channel 0 data |
| 0x43 | command register |

### pic (0x20-0x21, 0xA0-0xA1)

| port | deskripsi |
|------|-----------|
| 0x20 | master pic command |
| 0x21 | master pic data |
| 0xA0 | slave pic command |
| 0xA1 | slave pic data |

### ata/ide (0x1F0-0x1F7)

| port | deskripsi |
|------|-----------|
| 0x1F0 | data port (16-bit) |
| 0x1F2 | sector count |
| 0x1F3 | LBA low |
| 0x1F4 | LBA mid |
| 0x1F5 | LBA high |
| 0x1F6 | drive/head |
| 0x1F7 | status/command |

### cmos/rtc (0x70-0x71)

| port | deskripsi |
|------|-----------|
| 0x70 | address register |
| 0x71 | data register |

---

## troubleshooting

### port gak response

- cek port address bener
- cek device enabled
- cek I/O permission (kalo ada)

### data corrupt

- cek data size (byte/word/dword)
- cek timing (mungkin butuh io_wait)
- cek device ready

### system hang

- cek infinite loop di wait function
- cek device status flags
- cek timeout handling

---

**kembali ke:** [driver →](readme.md)
