# 05. driver

dokumentasi ini ngebahas semua driver yang ada di OaSis.

## daftar isi

- [overview](#overview)
- [vga driver](vga.md)
- [keyboard driver](keyboard.md)
- [timer driver](timer.md)
- [ata/ide driver](ata.md)
- [i/o port](io.md)

---

## overview

driver di OaSis adalah layer yang ngasih abstraction buat hardware. kernel gak perlu tau detail hardware, cukup panggil driver API.

### struktur driver

```
┌─────────────────────────────────────┐
│         kernel / aplikasi           │
├─────────────────────────────────────┤
│         driver api                  │  ← interface
├─────────────────────────────────────┤
│         driver implementation       │  ← logic
├─────────────────────────────────────┤
│         hardware                    │  ← device
└─────────────────────────────────────┘
```

### driver yang tersedia

| driver | fungsi | file |
|--------|--------|------|
| vga | text mode display | `src/drivers/vga.c` |
| keyboard | input dari keyboard ps/2 | `src/drivers/keyboard.c` |
| timer | system timer (pit) | `src/drivers/timer.c` |
| ata/ide | disk storage | `src/drivers/ata.c` |
| i/o | port i/o operations | `src/drivers/io.c` |

## prinsip desain

### 1. abstraction
driver hide detail hardware dari kernel. kernel cuma perlu tau "tulis karakter ke layar", bukan "tulis ke memory address 0xB8000".

### 2. consistency
semua driver punya API yang similar:
- `driver_init()` - inisialisasi
- `driver_operation()` - operasi spesifik

### 3. error handling
driver return error code kalo ada masalah, bukan crash.

### 4. interrupt-driven
driver yang butuh response cepat (keyboard, timer) pake interrupt, bukan polling.

## file terkait

- `src/drivers/vga.c` - vga driver
- `src/drivers/keyboard.c` - keyboard driver
- `src/drivers/timer.c` - timer driver
- `src/drivers/ata.c` - ata/ide driver
- `src/drivers/io.c` - i/o port operations
- `include/drivers/*.h` - header files

selanjutnya: [vga driver →](vga.md)
