# build system

dokumentasi ini ngebahas gimana cara build dan run OaSis.

## daftar isi

- [overview](#overview)
- [prasyarat](#prasyarat)
- [makefile](#makefile)
- [cara build](#cara-build)
- [cara run](#cara-run)
- [troubleshooting](#troubleshooting)

---

## overview

**build system** di OaSis pake `make` buat automate proses compile dan link.

### proses build

```
source files (.c, .asm)
  ↓
compile (gcc, nasm)
  ↓
object files (.o)
  ↓
link (ld)
  ↓
kernel binary (kernel.bin)
```

## prasyarat

tools yang dibutuhin buat build OaSis:

### wajib

**gcc** (gnu compiler collection)
```bash
sudo apt install gcc
```
- compiler buat C code
- support flag `-m32` buat 32-bit

**nasm** (netwide assembler)
```bash
sudo apt install nasm
```
- assembler buat x86 assembly
- output format: elf32

**make**
```bash
sudo apt install make
```
- build automation tool

**qemu-system-i386**
```bash
sudo apt install qemu-system-x86
```
- emulator buat run OS

### optional

**grub-mkrescue** (buat bikin bootable iso)
```bash
sudo apt install grub-pc-bin xorriso
```

## makefile

makefile adalah script yang define gimana cara build project.

### struktur makefile OaSis

```makefile
# compiler
CC = gcc
CFLAGS = -m32 -nostdlib -fno-builtin -fno-stack-protector -ffreestanding -Wall -Wextra -Iinclude

# assembler
AS = nasm
ASFLAGS = -f elf32

# linker
LD = ld
LDFLAGS = -m elf_i386 -T src/boot/linker.ld

# source files
SOURCES_CORE = src/kernel/main.c \
               src/kernel/memory.c \
               src/kernel/idt.c \
               src/kernel/task.c \
               src/kernel/syscall.c

SOURCES_DRIVERS = src/kernel/drivers/vga.c \
                  src/kernel/drivers/keyboard.c \
                  src/kernel/drivers/timer.c \
                  src/kernel/drivers/disk.c

SOURCES_FS = src/kernel/fs/vfs.c

SOURCES_APPS = src/apps/editor.c

SOURCES_ASM = src/boot/entry.asm

SOURCES_C = $(SOURCES_CORE) $(SOURCES_DRIVERS) $(SOURCES_FS) $(SOURCES_APPS)
SOURCES = $(SOURCES_C) $(SOURCES_ASM)

# object files
OBJECTS = $(SOURCES:.c=.o)
OBJECTS := $(OBJECTS:.asm=.o)

# target
KERNEL = kernel.bin

# default target
all: $(KERNEL)

# link
$(KERNEL): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

# compile c
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# compile asm
%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

# run
run: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL)

# clean
clean:
	rm -f $(OBJECTS) $(KERNEL)

# iso
iso: $(KERNEL)
	mkdir -p iso/boot
	cp $(KERNEL) iso/boot/
	grub-mkrescue -o oasis.iso iso
	rm -rf iso

.PHONY: all run clean iso
```

### variable penting

**CC**: C compiler (gcc)

**CFLAGS**: flags buat C compiler
- `-m32`: compile 32-bit
- `-nostdlib`: gak pake standard library
- `-fno-builtin`: disable builtin functions
- `-ffreestanding`: freestanding environment
- `-Wall -Wextra`: enable warnings
- `-Iinclude`: include directory

**AS**: assembler (nasm)

**ASFLAGS**: flags buat assembler
- `-f elf32`: output format elf32

**LD**: linker (ld)

**LDFLAGS**: flags buat linker
- `-m elf_i386`: 32-bit elf
- `-T src/boot/linker.ld`: linker script

### targets

**all**: default target, build kernel.bin

**run**: build dan run di qemu

**clean**: hapus semua object files dan kernel.bin

**iso**: bikin bootable iso image

## cara build

### build kernel

```bash
make
```

atau explicit:

```bash
make all
```

**output:**
- `kernel.bin`: kernel binary

### build dari scratch

```bash
make clean
make
```

### build iso

```bash
make iso
```

**output:**
- `oasis.iso`: bootable iso image

## cara run

### run langsung (paling gampang)

```bash
make run
```

ini bakal:
1. build kernel.bin (kalo belum)
2. run qemu dengan kernel.bin

### run manual

```bash
# build dulu
make

# run dengan qemu
qemu-system-i386 -kernel kernel.bin
```

### run iso

```bash
# build iso
make iso

# run iso di qemu
qemu-system-i386 -cdrom oasis.iso
```

### qemu options

```bash
# dengan serial output
qemu-system-i386 -kernel kernel.bin -serial stdio

# dengan disk image
qemu-system-i386 -kernel kernel.bin -hda disk.img

# dengan memory 128 MB
qemu-system-i386 -kernel kernel.bin -m 128

# fullscreen
qemu-system-i386 -kernel kernel.bin -fullscreen

# tanpa gui (headless)
qemu-system-i386 -kernel kernel.bin -nographic
```

## troubleshooting

### error: gcc not found

**solusi:**
```bash
sudo apt install gcc
```

### error: nasm not found

**solusi:**
```bash
sudo apt install nasm
```

### error: qemu not found

**solusi:**
```bash
sudo apt install qemu-system-x86
```

### error: undefined reference

**penyebab:** function di-declare tapi gak di-define

**solusi:**
- cek semua source files ada di makefile
- cek function implementation ada

### error: multiple definition

**penyebab:** function/variable di-define lebih dari sekali

**solusi:**
- cek gak ada duplicate definitions
- pake `extern` buat declarations di header

### warning: implicit declaration

**penyebab:** function dipanggil tanpa declaration

**solusi:**
- include header yang declare function
- atau declare function sebelum pake

### kernel gak boot

**penyebab:** banyak kemungkinan
- boot signature missing
- invalid multiboot header
- infinite loop di init

**solusi:**
- cek `entry.asm` punya signature 0xAA55
- cek multiboot header valid
- tambah debug output di init functions
- run dengan qemu serial: `qemu-system-i386 -kernel kernel.bin -serial stdio`

### keyboard gak response

**penyebab:** interrupt handler belum di-setup

**solusi:**
- cek `keyboard_init()` dipanggil
- cek IRQ 1 enabled di PIC
- cek IDT entry buat IRQ 1

### screen blank

**penyebab:** vga driver belum di-init atau error di init

**solusi:**
- cek `vga_init()` dipanggil di awal
- cek vga memory address (0xB8000)
- tambah simple test: `vga_puts("test\n");`

### make: nothing to be done

**penyebab:** semua files up-to-date

**solusi:**
```bash
make clean
make
```

---

## development workflow

### typical workflow

```bash
# 1. edit code
vim src/kernel/main.c

# 2. build
make

# 3. run dan test
make run

# 4. kalo ada error, fix dan ulangi
```

### clean build

```bash
make clean
make
make run
```

### debug build

tambah debug flags di makefile:

```makefile
CFLAGS = -m32 -g -O0 -nostdlib -fno-builtin ...
```

- `-g`: include debug symbols
- `-O0`: no optimization (easier debugging)

---

## performance

### build time

typical build time (clean build):
- compile: ~2-3 seconds
- link: ~1 second
- total: ~3-4 seconds

### kernel size

typical kernel.bin size:
- ~20-50 KB (depends on features)

---

## ci/cd (future)

buat automation:

```yaml
# .github/workflows/build.yml
name: build

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: install dependencies
        run: sudo apt install gcc nasm qemu-system-x86
      - name: build
        run: make
      - name: test
        run: make run
```

---

**kembali ke:** [dokumentasi →](../readme.md)
