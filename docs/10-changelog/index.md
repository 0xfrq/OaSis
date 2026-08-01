---
layout: default
title: Changelog
description: Track notable OaSis fixes, features, and documentation updates.
content_type: reference
audience: contributors
---

# Changelog

## 2026-08-01: RTL8139 networking and protocol stack

Added a QEMU-tested kernel networking path:

- PCI bus scanning and Realtek RTL8139 discovery.
- RTL8139 reset, MAC discovery, DMA buffer translation, four TX descriptors, and RX ring polling.
- Ethernet frame construction and EtherType dispatch.
- ARP request, reply, cache, and resolution support.
- IPv4 header construction, checksum validation, and protocol registration.
- ICMP echo request and reply handling.
- `pci`, `nicinfo`, `arp`, and `ping <ip>` shell commands.
- Host-side checksum and malformed IPv4 packet tests in `test_network.c`.

The implementation targets QEMU's `-nic user,model=rtl8139` configuration. It uses static guest values and polling. TCP, UDP, DHCP, sockets, network syscalls, and interrupt-driven NIC processing remain future work.

## 2026-08-01: Documentation refresh

Updated the README and documentation site with the networking architecture, reproducible test workflow, troubleshooting guidance, source map, and VGA-to-GUI roadmap. Planned GUI work is now labeled separately from the current text-mode implementation.

## 2026-06-29: Assembler `parse_int` whitespace fix

**Bug**: The assembler's `parse_int()` function did not handle leading whitespace. The `occ` code generator emits parameter loads such as `[ebp + 8]`. Without whitespace handling, `[ebp + 8]` was assembled as `[ebp + 0]`.

**Fix**: Skip leading whitespace in `parse_int()` and tolerate trailing whitespace in hexadecimal and decimal parsing.

**File**: `src/kernel/apps/asm.c`

## 2026-06-29: Ring 3 page fault fixes

**Bug**: `user` programs could page fault when calling user-space library wrappers because the user page directory did not expose the required identity-mapped pages.

**Fixes**:

- Mark the required identity-map entries as user-accessible.
- Remap user-mode symbols such as `_printf` to `_usr_printf`.
- Use `asm_assemble_user()` for ring 3 programs.

**Files**: `src/kernel/core/paging.c`, `src/kernel/apps/asm.c`, `src/kernel/tasks/task_user.c`, `include/asm.h`

## 2026-06-29: Code-generation entry trampoline

Generated programs now begin with `call _main` followed by `ret`, so execution reaches `main` instead of starting at the first helper function.

**File**: `src/kernel/lib/codegen.c`

## 2026-06-29: Assignment code-generation fall-through

Added the missing `break` after assignment generation so assignment statements no longer fall through into `if` generation.

**File**: `src/kernel/lib/codegen.c`
