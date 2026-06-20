---
layout: default
title: Kernel
---

# 04. Kernel

This section covers the OaSis kernel core: GDT, IDT, paging, memory management, task scheduling, and system calls.

## Table of Contents

- [Memory Management](memory.md)
- [Interrupt Handling](interrupt.md)
- [Task Scheduling](task.md)
- [System Calls](syscall.md)

## Overview

The kernel has four main responsibilities:

### 1. Memory Management
- **Physical Memory Manager (PMM)**: Bitmap-based allocator tracking each 4KB page.
  The PMM bitmap is 1MB (8M entries) covering up to 32GB of physical memory.
  `pmm_alloc_page()` scans the bitmap for a free page, marks it used, and returns the physical address.
- **Paging**: 4KB page tables with identity mapping for low memory (0-4MB) and higher-half kernel mapping at `0xC0000000+`.
  `page_map(virt, phys, flags)` creates page table entries automatically, allocating new page tables from a static pool when needed.
- **Process Isolation**: `paging_create_user_dir()` clones the kernel page directory for user tasks,
  stripping `PTE_USER` from kernel pages (identity map, higher-half) and adding it to user pages (code, stack, heap).

### 2. Interrupt Handling
- **IDT**: 256 entries covering 32 CPU exceptions, 16 hardware IRQs, and the int 0x80 syscall gate.
- **Exception handlers**: CPU exceptions (0-31) are handled by `isr_common_stub` which saves all registers
  via PUSHA, pushes the error code and interrupt number, calls the C handler `interrupt_handler()`, then restores via POPA and IRET.
- **IRQ handlers**: Hardware interrupts (32-47) with specific handlers for timer (IRQ0) and keyboard (IRQ1),
  sending EOI to the PIC before returning.
- **int 0x80 syscall gate**: Set with DPL=3 (0xEF) to allow user-mode code (ring 3) to invoke system calls.

### 3. Task Scheduling
- Round-robin scheduler driven by the timer IRQ (100Hz).
- Tasks are stored in a fixed-size array (`TASK_MAX = 16`) linked in a circular list.
- `task_switch()` cycles `current_task = current_task->next` and updates CR3 if the task has a dedicated page directory.
- On timer interrupt, `timer_interrupt_handler()` calls `task_switch()`.

### 4. System Calls
- 23 system calls dispatched by `syscall_dispatch()` based on the number in `eax`.
- Arguments passed in `ebx`, `ecx`, `edx`; return value in `eax`.
- The `int_80_wrapper` in `interrupt.asm` detects origin ring (0 vs 3) by checking `[esp+36]`
  (CS value) and handles the different iret frame layouts accordingly.
- For ring 3 calls, the handler can redirect iret to `user_return_to_shell` when `SYSCALL_USER_EXIT` is called.
