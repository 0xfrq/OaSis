---
layout: default
title: Overview
---

# OaSis OS Documentation

## What is OaSis?

OaSis is a **32-bit educational operating system** for x86 (i386) architecture, built entirely from scratch. It boots via GRUB, runs in protected mode with paging, and includes a shell, filesystem, C compiler, built-in assembler, user mode (ring 3) execution, and process isolation.

**Target audience:** Students, hobbyists, and anyone who wants to understand how an OS works at the lowest level.

## Project Status

| Component | Status | Details |
|-----------|--------|---------|
| Boot + GDT + IDT | DONE | GRUB multiboot, 32-bit PM, GDT with ring 0/3 segments, TSS |
| Drivers | DONE | VGA text mode (80x25), PS/2 keyboard, PIT timer, ATA/IDE, PIC |
| Paging + PMM | DONE | 4KB pages, bitmap PMM, higher-half kernel mapping |
| Process Isolation | DONE | Per-task page directory, CR3 switching, PTE_USER control |
| OAFS Filesystem | DONE | Inode-based, 1024 inodes, indirect blocks, multi-block dirs |
| Syscalls | DONE | 23 syscalls via int 0x80, ring 0 + ring 3 dispatch |
| Kernel Heap | DONE | Free-list allocator with splitting and coalescing |
| occ Compiler | DONE | Subset C: int, char, for/while/if, function params |
| Built-in Assembler | DONE | x86 32-bit, times, labels, segment regs, external syms |
| User Mode (Ring 3) | DONE | iret to ring 3, user page dir, SYSCALL_USER_EXIT |
| Logging | DONE | Circular buffer, dmesg, auto-log exceptions |
| Shell + Utilities | DONE | ls (color), cd, cat, write, echo, hexdump, edit, nasm, occ, user |

## Architecture Overview

The kernel architecture follows a **monolithic design** with layered subsystems:

```
Boot (GRUB)
  -> entry.asm (stack, GDT reload)
    -> kernel_main()
      -> GDT init (ring0 + ring3 + TSS)
      -> IDT init (exceptions + IRQs + int 0x80)
      -> PIC init (IRQ routing)
      -> Timer init (100Hz scheduler tick)
      -> Keyboard init (PS/2)
      -> Memory init (E820, PMM, Paging)
      -> Task init (TCB array, scheduler)
      -> FD init (file descriptors)
      -> Block init (ATA/IDE cache)
      -> VFS init (OAFS load or format)
      -> Syscall init (int 0x80 gate with DPL=3)
      -> Shell loop (keyboard -> command -> execute)
```

## Memory Layout

```
0x00000000 - 0x003FFFFF   Identity mapped (kernel code, BSS, stack)
0x00400000 - 0x007FFFFF   Extended kernel mapping (page tables)
0x00800000 - 0x00EFFFFF   User code pages (ring 3 executables)
0x00F00000 - 0x00FFFFFF   User stack (ring 3, 16KB)
0x01000000 - 0x02000000   User heap (brk expansion)
0x02000000 - 0x03000000   Kernel heap (kmalloc arena)
0x40000000                CODE_VIRT (assembler code buffer, 16KB)
0xC0000000+               Higher-half kernel mapping
```

## Build System

The build uses a simple Makefile with gcc (cross-compiler to i386) and NASM.

### Prerequisites

```
sudo apt install gcc nasm grub2-common xorriso qemu-system-x86
```

### Commands

```
make        # build kernel.bin + oasis.iso
make clean  # remove build artifacts
make run    # boot in QEMU
```

### Build Output

- `kernel.bin` — ELF binary loaded by GRUB at 1MB physical
- `oasis.iso` — ISO image with GRUB bootloader

The linker script (`src/boot/linker.ld`) places the kernel at 1MB with `.text`, `.rodata`, `.data`, `.bss` sections.
