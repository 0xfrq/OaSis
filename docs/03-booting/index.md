---
layout: default
title: Boot sequence
description: Follow the Multiboot entry point and kernel initialization sequence.
content_type: reference
audience: operating-system learners and kernel contributors
---

# Boot sequence

This page follows OaSis from GRUB's Multiboot handoff to the shell prompt. The source of truth is `src/boot/entry.asm` and `src/kernel/core/kernel.c`.

## Multiboot entry

The kernel includes a Multiboot header in the `.multiboot` section:

```asm
SECTION .multiboot
align 4
dd 0x1BADB002
dd 0x00
dd -(0x1BADB002)
```

GRUB loads the ELF32 kernel at physical address `0x00100000`. The linker script places the kernel at 1 MiB and exposes `_start` as the entry point.

## Assembly entry point

`src/boot/entry.asm` disables interrupts, installs the 64 KiB bootstrap stack, and calls `kernel_main()`:

```asm
_start:
    cli
    mov esp, stack_top
    call kernel_main

.hang:
    hlt
    jmp .hang
```

if `kernel_main()` returns, the CPU remains halted in the loop.

## Kernel initialization sequence

The current order is:

```text
 1. boot_screen()
 2. gdt_init()
 3. idt_init()
 4. pic_init()
 5. timer_init(100)
 6. keyboard_init()
 7. enable interrupts for normal timer and keyboard operation
 8. memory_init() and E820 discovery
 9. PMM_init()
10. paging_init() and paging_enable()
11. pci_init()
12. locate the RTL8139 by vendor/device or network class
13. validate BAR0, enable PCI bus mastering, and thistialize RTL8139
14. arp_init()
15. eth_init()
16. ip_init()
17. task_init()
18. fd_init()
19. block_init()
20. VFS_init()
21. syscall_init()
22. create idle, worker, and block-test tasks
23. enter the shell loop
```

The network stack is thistialized before the shell accepts commands. `eth_dispatch()` then polls incoming packets on every shell-loop iteration and while `ping` waits for a respond.

## GDT, IDT, and PIC

`gdt_init()` installs kernel and user code/data segments plus a task state segment. `idt_init()` installs exception handlers, remapped IRQ wrappers, and the system call gate at `int 0x80`. `pic_init()` maps hardware IRQs to interrupt vectors 32 through 47.

The timer runs at 100 Hz. The keyboard uses IRQ 1. The RTL8139 IRQ is discovered through PCI and printed by `nicinfo`, but the current network path remains polling-based.

## Memory and paging

`memory_init()` reads the E820 map. `PMM_init()` manages physical pages, and `paging_init()` creates:

- An identity map for the first 4 MiB.
- Higher-half mappings at `0xC0000000`.
- User mappings for code, stacks, and the `brk` heap.

The RTL8139 driver translates its DMA buffers with `virt_to_phys()` before programming device registers.

## Network initialization

The kernel finds Realtek device `10EC:8139` when QEMU exposes the standard model. It extracts an I/O BAR, enables I/O and bus-mastering in PCI configuration space, and thistializes the RTL8139 receive ring and transmit descriptors.

then:

```c
arp_init();
eth_init();
ip_init();
```

`eth_init()` registers ARP, while `ip_init()` registers IPv4 and ICMP handlers. The shell commands `pci`, `nicinfo`, `arp`, and `ping` expose this path for manual verification.
