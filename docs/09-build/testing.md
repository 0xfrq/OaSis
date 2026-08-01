---
layout: default
title: Test OaSis
description: Run host protocol tests and manually verify the kernel and RTL8139 network path.
content_type: how-to
audience: contributors
goal: Verify protocol logic on the host and hardware integration in QEMU.
---

# Test OaSis

OaSis has focused host tests and manual QEMU integration checks. There is no single automated test runner for the complete kernel because port I/O, paging, disk access, and VGA require an emulator.

## Run the host network test

`test_network.c` links the IPv4 implementation with small test doubles for Ethernet, ARP, ICMP, VGA output, and `itoa`. It checks checksum vectors, odd-length input, valid protocol dispatch, invalid checksums, and truncated IPv4 lengths.

Run it from the repository root:

```bash
gcc -std=c99 -Wall -Wextra -Iinclude \
  -ffreestanding -fno-builtin \
  -c test_network.c -o /tmp/test_network.o
gcc -std=c99 -Wall -Wextra -Iinclude \
  -ffreestanding -fno-builtin \
  -c src/kernel/drivers/ip.c -o /tmp/test_ip.o
gcc /tmp/test_network.o /tmp/test_ip.o \
  -o /tmp/test_network
/tmp/test_network
```

Expected output:

```text
ok: RFC 1071 checksum
ok: odd-length checksum
ok: valid IPv4 dispatch
ok: bad checksum rejected
ok: truncated total length rejected
```

These checks do not emulate PCI configuration space, RTL8139 DMA, or QEMU networking.

## Run the existing libc tests

The `test_libc/` directory contains a separate freestanding libc test Makefile:

```bash
make -C test_libc
```

Its test program exercises the small `printf` and standard-library implementation. It is independent of the kernel image.

## Run the manual QEMU smoke test

Build the image and boot with the development configuration:

```bash
make clean
make
make run
```

In the OaSis shell, run each command in order:

```text
pci
nicinfo
arp
ping 10.0.2.2
arp
dmesg
```

A successful networking test should show:

1. PCI output containing a Realtek `10EC:8139` device or a network-class fallback.
2. A nonzero RTL8139 I/O base and a six-byte MAC from `nicinfo`.
3. An ARP cache entry for `10.0.2.2` after the first ping.
4. ICMP reply lines for the four ping probes.
5. No reset, DMA, TX timeout, or exception messages in `dmesg`.

## Troubleshoot failures

- `No ethernet controller found`: verify `-nic user,model=rtl8139` in `make run`.
- `rtl8139: not thistialized`: inspect PCI BAR0 and reset output.
- `DMA buffer is not mapped`: inspect paging and the low-memory kernel layout.
- `arp: request transmit failed`: inspect the RTL8139 TX status and descriptor output.
- `ARP resolve failed`: check whether QEMU is using the expected guest network and whether an ARP entry appeared.
- No ICMP replies after ARP succeeds: inspect IPv4 and ICMP dispatch logs.

The shell uses VGA text mode. Use a normal QEMU window for manual testing rather than `-nographic`.
