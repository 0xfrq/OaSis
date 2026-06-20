---
layout: default
title: overview
---

# oasis os

sistem operasi 32-bit x86 edukasi, dibangun dari nol.

## status

| komponen | status |
|----------|--------|
| boot + gdt + idt | selesai |
| drivers (vga, keyboard, timer, ata, pic) | selesai |
| paging + pmm | selesai |
| process isolation (per-task cr3) | selesai |
| oafs filesystem | selesai |
| syscall layer (23 syscall) | selesai |
| kernel heap (kmalloc/kfree) | selesai |
| occ compiler (subset c) | selesai |
| built-in assembler | selesai |
| user mode (ring 3) | selesai |
| logging (dmesg) | selesai |
| shell + utilities | selesai |
| boot screen | selesai |

## build & run

```
sudo apt install gcc nasm grub2-common xorriso qemu-system-x86
make          # build kernel.bin + oasis.iso
make clean    # bersihin artifact
make run      # boot di qemu
```

## struktur direktori

```
src/
  boot/          entry.asm, linker.ld
  kernel/
    core/        kernel.c, gdt.c, paging.c, pmm.c, vga.c, memory.c
    drivers/     ata.c, block.c, idt.c, io.c, keyboard.c, pic.c, timer.c
    fs/          fd.c, vfs.c
    lib/         string.c, lexer.c, parser.c, codegen.c, klibc.c, heap.c, log.c, usrlib.c
    syscall/     syscall.c, interrupt.asm
    tasks/       task.c, task_user.c
    apps/        editor.c, asm.c
include/         header files
docs/            dokumentasi (jekyll)
Makefile         build system
```

## memory layout

```
0x00000000 - 0x003FFFFF   identity map (kernel + bss)
0x00800000 - 0x00EFFFFF   user code pages
0x00F00000 - 0x00FFFFFF   user stack
0x01000000 - 0x02000000   user heap (brk)
0x02000000 - 0x03000000   kernel heap (kmalloc)
0x40000000                CODE_VIRT (assembler buffer)
0xC0000000+               higher-half kernel
```
