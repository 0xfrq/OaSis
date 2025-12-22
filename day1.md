# Day 1 – Oasis OS: Booting a Real Kernel

## Goal of Day 1

The goal of Day 1 is not to build features. The goal is to **prove that our own kernel code executes on bare metal** (virtual hardware via QEMU).

By the end of Day 1, we must be able to say with confidence:

- Our kernel is loaded into memory
- The CPU jumps into our code
- We run in Ring 0 (kernel mode)
- C code executes without any operating system underneath
- We can write directly to hardware memory (VGA)

If this works, everything else becomes possible.

---

## Important Mindset

A black screen or a blinking cursor is **not a failure** in early OS development.

Silence usually means:

- The kernel is running
- The CPU is halted intentionally
- No drivers exist yet

This is normal.

---

## Toolchain Used (Ubuntu 16.04)

We are building a **freestanding kernel**, not a Linux program.

Required tools:

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  nasm \
  xorriso \
  grub-pc-bin \
  grub-common \
  qemu-system-x86
```

Verification:

```bash
nasm -v
ld --version
qemu-system-i386 --version
```

---

## Final Project Structure (Day 1)

This structure must not change later.

```
oasis/
├── boot/
│   └── entry.asm
├── kernel/
│   ├── kernel.c
│   └── linker.ld
├── iso/
│   └── boot/
│       └── grub/
│           └── grub.cfg
└── Makefile
```

---

## Boot Process Overview

This is the exact execution flow:

```
qemu / bios
    ↓
(kernel loaded by qemu or grub)
    ↓
entry.asm (_start)
    ↓
kernel_main() in c
    ↓
halt loop
```

There is no Linux, no syscalls, no libc.

---

## Entry Assembly (boot/entry.asm)

This is the first code that runs.

```asm
BITS 32

SECTION .multiboot
align 4
dd 0x1BADB002
dd 0x00
dd -(0x1BADB002)

SECTION .text
GLOBAL _start
EXTERN kernel_main

_start:
    cli
    mov esp, stack_top
    call kernel_main

.hang:
    hlt
    jmp .hang

SECTION .bss
align 16
resb 8192
stack_top:
```

### Explanation (Simple)

- Multiboot header allows bootloaders to recognize the kernel
- We disable interrupts (cli)
- We create our own stack
- We jump into C code
- We halt forever so the CPU does not crash

---

## Linker Script (kernel/linker.ld)

This tells the linker where to place the kernel in memory.

```ld
ENTRY(_start)

SECTIONS
{
    . = 1M;

    .multiboot : { *(.multiboot) }
    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
}
```

### Explanation

- Kernel starts at 1 MB (standard safe location)
- Multiboot section must be preserved
- Memory layout is fully controlled by us
- One small syntax error here can break everything

---

## Minimal Kernel (kernel/kernel.c)

This version proves that:

- C code executes
- VGA memory is writable
- The kernel is truly running

```c
void kernel_main(void) {
    volatile char* vga = (volatile char*)0xB8000;
    const char* msg = "WELCOME TO OASIS";
    int i = 0;

    while (msg[i]) {
        vga[i * 2] = msg[i];
        vga[i * 2 + 1] = 0x0F;
        i++;
    }

    while (1) {
        __asm__ __volatile__("hlt");
    }
}
```

### Explanation

- `0xB8000` is VGA text memory
- Each character takes 2 bytes (char + color)
- This bypasses BIOS completely
- If text appears, the kernel is alive

---

## GRUB Configuration (iso/boot/grub/grub.cfg)

```cfg
menuentry "OASIS" {
    multiboot /boot/kernel.bin
    boot
}
```

This is only used when booting via ISO. During early development, we can bypass GRUB.

---

## Makefile

```makefile
CC=gcc
LD=ld

CFLAGS=-m32 \
       -ffreestanding \
       -fno-stack-protector \
       -fno-pic \
       -nostdlib \
       -Wall -Wextra

LDFLAGS=-m elf_i386

all: iso

kernel.bin:
	nasm -f elf32 boot/entry.asm -o entry.o
	$(CC) $(CFLAGS) -c kernel/kernel.c -o kernel.o
	$(LD) $(LDFLAGS) -T kernel/linker.ld -o kernel.bin entry.o kernel.o

iso: kernel.bin
	cp kernel.bin iso/boot/
	grub-mkrescue -o oasis.iso iso

run:
	qemu-system-i386 -kernel kernel.bin -m 128M

clean:
	rm -f *.o *.bin *.iso
```

### Explanation

- Freestanding flags prevent GCC from assuming Linux
- No standard library is linked
- `kernel.bin` is a raw binary kernel
- `qemu -kernel` bypasses ISO and GRUB (used for sanity testing)

---

## How We Verified Success

We ran:

```bash
qemu-system-i386 -kernel kernel.bin -m 128M
```

We saw:

```
WELCOME TO OASIS
```

Even though BIOS text remained on screen.

This confirms:

- The kernel is executing
- `entry.asm` works
- Stack is valid
- C code runs
- Hardware memory access works

This is absolute proof of success.

---

## Common Confusion Explained

### Why Does BIOS Text Still Show?

Because:

- BIOS wrote to VGA first
- Our kernel did not clear the screen yet
- This is expected

### Why Does It Look Stuck?

Because:

- The kernel intentionally halts
- No timer interrupts exist yet
- No keyboard input exists yet
- This is correct behavior

---

## Final Day 1 Checklist

All of these are true:

- ✓ Kernel loads into memory
- ✓ CPU jumps into `_start`
- ✓ Stack is initialized
- ✓ C code runs
- ✓ VGA memory is written
- ✓ System does not crash or reboot

**Day 1 is complete.**