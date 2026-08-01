---
layout: default
title: Introduction
description: Learn what OaSis is, what it teaches, and how to start.
content_type: landing
audience: new contributors and operating-system learners
---

# Introduction

OaSis is a small 32-bit x86 operating system built from scratch. It is a learning project for studying boot code, protected mode, memory, tasks, filesystems, drivers, system calls, networking, and language tools.

## What you can learn

- How GRUB loads a Multiboot kernel.
- How the GDT, IDT, PIC, and TSS support protected mode.
- How physical memory, paging, and the kernel heap work.
- How tasks switch contexts and enter ring 3 user mode.
- How an inode filesystem stores files and directories.
- How device drivers communicate with keyboard, disk, VGA, PCI, and RTL8139 hardware.
- How Ethernet, ARP, IPv4, and ICMP fit together.
- How a lexer, parser, code generator, and assembler build programs.

## Project status

The kernel boots to a VGA text shell. User programs can run in ring 3 with process page directories and system calls. QEMU networking supports ARP and ICMP ping through the RTL8139 model.

A graphical interface, TCP, UDP, DHCP, sockets, and network syscalls are not implemented yet.

## Prerequisites

You need a Linux system with the 32-bit GCC toolchain, NASM, GRUB utilities, xorriso, and QEMU. See the [build guide](../09-build/) for installation commands.

## Next steps

1. [Build and test OaSis](../09-build/).
2. Read the [architecture guide](../02-arsitektur/).
3. Follow the [boot sequence](../03-booting/).
4. Study [networking internals](../05-driver/networking/).
5. Review the [GUI roadmap](../11-gui/).
