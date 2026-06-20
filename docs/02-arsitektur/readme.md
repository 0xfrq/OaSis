# 02. arsitektur

dokumentasi ini ngebahas gimana OaSis dirancang secara keseluruhan.

## daftar isi

- [gambaran umum](#gambaran-umum)
- [komponen utama](#komponen-komponen)
- [arsitektur booting](#arsitektur-booting)
- [arsitektur kernel](#arsitektur-kernel)
- [memory layout](#memory-layout)
- [keputusan desain](#keputusan-desain)

---

## gambaran umum

OaSis adalah **monolithic kernel** 32-bit yang jalan di mode protected (protected mode). semua komponen kernel jalan di ring 0 (kernel mode), termasuk driver dan filesystem.

**kenapa monolithic?**
- lebih simple buat OS edukasi
- performance lebih baik (gak ada context switch ke user space)
- easier debugging

**limitasi:**
- belum ada user mode (semua kode jalan di kernel)
- belum ada memory protection antar process
- belum ada virtual memory yang proper

## komponen utama

OaSis punya 5 komponen utama:

### 1. bootloader
- ngeload kernel ke memory
- setup multiboot header
- lompat ke entry point kernel

**file:** `src/boot/entry.asm`, `src/boot/linker.ld`

### 2. kernel core
- inisialisasi hardware
- memory management
- task scheduling
- system call interface

**file:** `src/kernel/main.c`

### 3. drivers
- vga (display)
- keyboard (input)
- timer (interrupt)
- ata/ide (disk)

**file:** `src/kernel/drivers/*.c`

### 4. filesystem
- oafs (oasis file system)
- vfs layer
- file dan directory operations

**file:** `src/kernel/fs/*.c`

### 5. shell
- command line interface
- built-in commands
- aplikasi user (editor)

**file:** `src/kernel/shell.c`, `src/apps/editor.c`

## arsitektur booting

```
┌─────────────────┐
│   grub/bios     │
└────────┬────────┘
         │ load kernel
         ▼
┌─────────────────┐
│  entry.asm      │  setup stack, multiboot header
└────────┬────────┘
         │ jump to main
         ▼
┌─────────────────┐
│  main.c         │  init semua subsystem
└────────┬────────┘
         │ spawn shell
         ▼
┌─────────────────┐
│  shell          │  interactive mode
└─────────────────┘
```

detail ada di [03-booting](../03-booting/readme.md)

## arsitektur kernel

kernel OaSis punya 4 layer utama:

```
┌─────────────────────────────────────┐
│         aplikasi (shell, editor)    │  user-facing
├─────────────────────────────────────┤
│         system call interface       │  API buat aplikasi
├─────────────────────────────────────┤
│         kernel core                 │  logic utama
├─────────────────────────────────────┤
│         drivers & hal               │  hardware abstraction
└─────────────────────────────────────┘
```

### aplikasi layer
- shell: command line interface
- editor: text editor sederhana
- future: bisa ditambah aplikasi lain

### system call interface
- syscall handler (int 0x80)
- syscall table
- parameter passing via registers

### kernel core
- memory manager (pmm, vmm)
- task scheduler
- interrupt handler
- vfs (virtual filesystem)

### drivers
- vga driver
- keyboard driver
- timer driver
- disk driver

## memory layout

OaSis pake flat memory model dengan paging:

```
0x00000000 - 0x000FFFFF   reserved (bios, vga buffer, dll)
0x00100000 - 0x002FFFFF   kernel code + data (2 MB)
0x00300000 - onwards      free memory (heap, stack, dll)
```

**vga buffer:** `0xB8000 - 0xBFFFF` (32 KB)

detail ada di [04-kernel/memory](../04-kernel/memory.md)

## keputusan desain

### 1. kenapa pake C?
- low-level access
- portable
- banyak dokumentasi
- standard buat OS development

### 2. kenapa monolithic?
- simpler buat learning
- performance lebih baik
- easier debugging

### 3. kenapa 32-bit?
- lebih simple dari 64-bit
- cukup buat fitur yang mau diajarin
- dokumentasi lebih banyak

### 4. kenapa custom filesystem (oafs)?
- belajar dari dasar
- simple dan understandable
- gampang di-extend

### 5. kenapa gak ada user mode?
- complexity terlalu tinggi buat scope ini
- fokus ke fundamental dulu
- bisa ditambah nanti sebagai advanced topic

---

selanjutnya: [booting →](../03-booting/readme.md)
