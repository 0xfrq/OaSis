---
layout: default
title: Architecture
description: Trace OaSis from boot code through kernel services, drivers, networking, and the shell.
content_type: conceptual
audience: operating-system learners and contributors
---

# Architecture

OaSis uses a monolithic kernel. Boot code, memory management, tasks, filesystems, drivers, system calls, networking, and the shell execute in one kernel address space.

## Kernel layers

```text
+--------------------------------------------------+
| Shell and command applications                   |
+--------------------------------------------------+
| Ethernet, ARP, IPv4, ICMP, and network commands  |
+--------------------------------------------------+
| System calls, file descriptors, and OAFS VFS     |
+--------------------------------------------------+
| Tasks, ring 3 user mode, and CR3 isolation       |
+--------------------------------------------------+
| Paging, PMM, kernel heap, and E820 memory map     |
+--------------------------------------------------+
| PCI, RTL8139, ATA, VGA, keyboard, timer, and PIC |
+--------------------------------------------------+
| GDT, IDT, Multiboot entry, and interrupt stubs   |
+--------------------------------------------------+
```

The current GUI is not a separate layer. Output uses VGA text mode, and future framebuffer work is tracked in the [GUI roadmap](../11-gui/).

## Boot and initialization order

The main initialization path is:

```text
grub
  -> src/boot/entry.asm
  -> kernel_main()
  -> GDT, IDT, PIC, timer, keyboard
  -> E820 memory map, PMM, paging
  -> PCI scan
  -> RTL8139 discovery and DMA setup
  -> ARP, Ethernet, and IPv4 registration
  -> tasks, file descriptors, block cache, OAFS, system calls
  -> shell loop
```

Interrupts are enabled for the timer and keyboard during normal operation. Network packets are processed by polling from `eth_dispatch()` rather than by a dedicated RTL8139 interrupt handler.

## Network data paths

Outgoing ICMP traffic follows this path:

```text
ping <ip>
  -> ip_send()
  -> arp_resolve()
  -> eth_send()
  -> rtl8139_send()
  -> QEMU user-mode networking
```

Incoming traffic follows this path:

```text
QEMU network
  -> RTL8139 RX ring
  -> rtl8139_poll()
  -> eth_dispatch()
  -> arp_handle_packet() or ip_handle_packet()
  -> icmp_handle_packet()
```

See [Networking internals](../05-driver/networking/) for packet layouts, register behavior, and current limitations.
