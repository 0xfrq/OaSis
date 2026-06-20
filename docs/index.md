---
layout: default
title: overview
---
# oasis os

sistem operasi 32-bit x86 edukasi, dibangun dari nol di atas arsitektur i386.

proyek ini mencakup beberapa file sumber (c, assembly, header) yang mencakup booting, protected mode, paging, multitasking, filesystem, syscall, compiler, assembler, dan user mode.

## status komponen

| komponen | status | file |
|----------|--------|------|
| boot + gdt + idt | selesai | entry.asm, gdt.c, idt.c |
| drivers (vga, keyboard, timer, ata, pic) | selesai | vga.c, keyboard.c, timer.c, ata.c, pic.c, io.c |
| paging + pmm + heap | selesai | paging.c, pmm.c, heap.c |
| process isolation (per-task cr3) | selesai | paging.c, task_user.c |
| oafs filesystem (indirect + multi-block) | selesai | vfs.c, fd.c |
| syscall layer (syscall, ring 0 + ring 3) | selesai | syscall.c, interrupt.asm |
| task scheduler (round-robin, cr3 switching) | selesai | task.c |
| kernel heap (free-list, splitting, coalescing) | selesai | heap.c |
| occ compiler (int, char, for, if, while, params, array) | selesai | lexer.c, parser.c, codegen.c |
| built-in assembler (x86 32-bit, times, ext syms) | selesai | asm.c |
| user mode (ring 3, iret redirect, exit handling) | selesai | task_user.c, interrupt.asm |
| klibc (printf, scanf, dll untuk kernel mode) | selesai | klibc.c |
| usrlib (printf, malloc via syscall) | selesai | usrlib.c |
| logging (circular buffer, dmesg, auto-exception log) | selesai | log.c |
| shell + utilities (ls, cd, cat, write, echo, hexdump) | selesai | kernel.c |
| text editor (nano-like, tab = 4 spasi) | selesai | editor.c |
| boot screen (oasis os di tengah, loading dots) | selesai | kernel.c |

## build

```
sudo apt install gcc nasm grub2-common xorriso qemu-system-x86
make # kernel.bin + oasis.iso
make clean # bersihin semua .o dan binary
make run # qemu-system-i386 -kernel kernel.bin
```

## struktur direktori

```
src/
 boot/ entry.asm (entry point, stack 64kb), linker.ld
 kernel/
 core/ kernel.c (shell), gdt.c, paging.c, pmm.c, vga.c, memory.c (e820)
 drivers/ ata.c (pio), block.c (cache), idt.c (interrupt handler), io.c (port io)
 keyboard.c (ps/2), pic.c (programmable interrupt controller), timer.c (pit)
 fs/ fd.c (file descriptor), vfs.c (oafs inode-based)
 lib/ string.c, lexer.c, parser.c, codegen.c (occ compiler)
 klibc.c (kernel libc), heap.c (kmalloc), log.c (logging), usrlib.c (user lib)
 syscall/ syscall.c (syscall dispatcher), interrupt.asm (irq + syscall handler)
 tasks/ task.c (scheduler, tcb), task_user.c (ring 3 task creation)
 apps/ editor.c (text editor), asm.c (built-in assembler)
include/ semua header files
docs/ dokumentasi ini
Makefile build system
iso/ grub config + kernel.bin untuk iso
```

## memory layout

```
0x00000000 - 0x003FFFFF identity map (kernel binary, bss, stack, page tables)
0x00400000 - 0x007FFFFF reserved (kernel mapping extension)
0x00800000 - 0x00EFFFFF area buat user code pages
0x00F00000 - 0x00FFFFFF user stack (16kb per task)
0x01000000 - 0x01FFFFFF user heap (brk, sampai 16mb)
0x02000000 - 0x02FFFFFF kernel heap (kmalloc arena, 16mb)
0x40000000 CODE_VIRT (assembler output, 16kb multi-page)
0xC0000000-0xC0400000 higher-half kernel mapping
```

## gdt layout

| selector | segment | ring | base | limit | access |
|----------|---------|------|------|-------|--------|
| 0x00 | null | - | 0 | 0 | 0 |
| 0x08 | kernel code | 0 | 0 | 4gb | 0x9A (present, ring0, code, exec/read) |
| 0x10 | kernel data | 0 | 0 | 4gb | 0x92 (present, ring0, data, read/write) |
| 0x18 | user code | 3 | 0 | 4gb | 0xFA (present, ring3, code, exec/read) |
| 0x20 | user data | 3 | 0 | 4gb | 0xF2 (present, ring3, data, read/write) |
| 0x28 | tss | 0 | &tss | sizeof(tss) | 0x89 (present, ring0, available tss) |
