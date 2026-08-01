---
layout: default
title: OaSis OS documentation
description: Learn how OaSis boots, manages memory, runs user programs, and connects to QEMU networking.
content_type: landing
audience: contributors and operating-system learners
goal: Build and test OaSis, then navigate its kernel subsystems.
---

# OaSis OS documentation

This site explains how OaSis boots, manages hardware, runs user programs, stores files, and communicates through a QEMU RTL8139 network device. Start with the build guide if you want to run the system, or use the architecture and driver pages to study the implementation.

## Start here

- [Build and test](09-build/): install tools, build the kernel, run QEMU, and verify networking.
- [Architecture](02-architecture/): follow the boot sequence and runtime data paths.
- [Networking](05-driver/networking/): understand PCI, RTL8139, Ethernet, ARP, IPv4, and ICMP.
- [Shell commands](07-shell/): use the filesystem, compiler, diagnostics, and network commands.
- [GUI roadmap](11-gui/): see what is planned beyond the current VGA text console.

## Implemented subsystems

| Subsystem | status | Main source |
| --- | --- | --- |
| Boot and protection | Implemented | `src/boot/entry.asm`, `gdt.c`, `idt.c` |
| memory | Implemented | `memory.c`, `PMM.c`, `paging.c`, `heap.c` |
| Tasks and user mode | Implemented | `task.c`, `task_user.c`, `syscall.c` |
| OAFS filesystem | Implemented | `VFS.c`, `fd.c`, `block.c` |
| Shell and applications | Implemented | `kernel.c`, `editor.c`, `asm.c` |
| `occ` compiler | Implemented subset | `lexer.c`, `parser.c`, `codegen.c` |
| PCI and RTL8139 | Implemented for QEMU | `pci.c`, `rtl8139.c` |
| Ethernet and ARP | Implemented | `ethernet.c`, `arp.c` |
| IPv4 and ICMP | Implemented subset | `ip.c`, `icmp.c` |
| Host protocol tests | Implemented | `test_network.c` |
| GUI | Planned | No framebuffer or compositor exists yet |

## Network quickstart

Build and run from the repository root:

```bash
make clean
make
make run
```

then run these commands in the OaSis shell:

```text
pci
nicinfo
arp
ping 10.0.2.2
arp
```

The default QEMU guest address is `10.0.2.15`. The network stack uses a static configuration from `include/netcfg.h`, sends Ethernet frames through the RTL8139 driver, and polls incoming frames from the shell loop.

## Current scope

OaSis currently supports Ethernet, ARP, IPv4 headers, ICMP echo requests and replies, and a kernel-level `ping` command. It does not yet provide DHCP, routing, TCP, UDP, DNS resolution, sockets, network syscalls, or interrupt-driven NIC reception.

The display is also text-only. The next GUI milestones are framebuffer discovery, bitmap font rendering, mouse input, a compositor, a graphical terminal, and user-space GUI APIs.

## Documentation map

- [Introduction](01-introduction/)
- [Architecture](02-architecture/)
- [Boot sequence](03-booting/)
- [Kernel](04-kernel/)
- [drivers](05-driver/)
- [Networking internals](05-driver/networking/)
- [filesystem](06-filesystem/)
- [Shell](07-shell/)
- [Applications](08-apps/)
- [Build](09-build/)
- [Testing](09-build/testing/)
- [Changelog](10-changelog/)
- [GUI roadmap](11-gui/)
