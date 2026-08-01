---
layout: default
title: Networking internals
description: Understand the PCI, RTL8139, Ethernet, ARP, IPv4, and ICMP path in OaSis.
content_type: reference
audience: kernel contributors and operating-system learners
---

# Networking internals

OaSis implements a small kernel-owned IPv4 stack for QEMU's RTL8139 network model. This page follows a packet from PCI discovery to an ICMP reply and identifies the boundaries that are not implemented yet.

## Supported scope

The current stack includes:

- PCI configuration-space scanning on bus 0.
- RTL8139 I/O-mode initialization and DMA buffers.
- Ethernet frames with ARP and IPv4 EtherTypes.
- An eight-entry ARP cache with request and reply handling.
- IPv4 header construction, checksum validation, and protocol dispatch.
- ICMP echo requests and replies.
- Polling-based receive processing through `eth_dispatch()`.

TCP, UDP, DHCP, DNS resolution, routing tables, sockets, network syscalls, and NIC interrupt handling are not implemented.

## QEMU network configuration

`make run` attaches the guest to QEMU user-mode networking with an RTL8139 model:

```text
-nic user,model=rtl8139
```

The static guest values are defined in `include/netcfg.h`:

| Value | Address |
| --- | --- |
| Guest IP | `10.0.2.15` |
| Gateway | `10.0.2.1` |
| Netmask | `255.255.255.0` |
| DNS constant | `10.0.2.3` |

The stack does not negotiate these values. Use a network environment that matches them.

## PCI discovery

`src/kernel/drivers/pci.c` accesses PCI configuration space through ports `0xCF8` and `0xCFC`. The current scan checks devices on bus 0 and records vendor ID, device ID, class, subclass, IRQ, and BAR values.

`kernel_main()` first looks for Realtek vendor/device `10EC:8139`. If that lookup fails, it falls back to a network controller with class `0x02` and subclass `0x00`. For an I/O BAR, it masks BAR0 with `0xFFFC`, enables PCI I/O and bus mastering, and passes the resulting port base to `rtl8139_init()`.

Run `pci` in the shell to repeat the scan and print the discovered devices.

## RTL8139 driver

The driver in `src/kernel/drivers/rtl8139.c` performs these steps:

1. Reset the controller with a bounded timeout.
2. Read the six-byte MAC address from IDR0 through IDR5.
3. Convert aligned RX and TX buffer addresses with `virt_to_phys()`.
4. Program the receive buffer, receive configuration, and transmit configuration.
5. Initialize the receive consumer pointer with the RTL8139 16-byte CAPR gap.
6. Enable transmit and receive in the command register.
7. Submit frames through four TX descriptor buffers and check completion status.
8. Poll the RX ring, validate status and length, copy the frame, and advance CAPR.

The receive ring uses an 8 KiB hardware window. Each descriptor starts with a four-byte header:

```text
status:  16-bit little-endian
length:  16-bit little-endian, including the four-byte Ethernet CRC
frame:   Ethernet data
```

The shell command `nicinfo` prints the I/O base, IRQ value, MAC address, command state, interrupt state, current buffer pointer, and software RX offset.

The IRQ value is discovered and displayed, but the current network path is polling-based. `eth_dispatch()` runs during the shell loop and while ARP or ping waits for packets.

## Ethernet layer

`src/kernel/drivers/ethernet.c` builds a 14-byte Ethernet header:

```text
6 bytes destination MAC
6 bytes source MAC
2 bytes EtherType in network byte order
```

Supported EtherTypes are:

| EtherType | Handler |
| --- | --- |
| `0x0806` | ARP |
| `0x0800` | IPv4 |

`eth_dispatch()` polls the RTL8139, rejects frames shorter than 14 bytes or addressed to another MAC, then invokes the registered handler for the EtherType.

## ARP resolution

`src/kernel/drivers/arp.c` stores up to eight IP-to-MAC entries. On a cache miss, `arp_resolve()` sends a broadcast request and polls for up to approximately 500 ms. A valid reply adds the sender to the cache and allows the original IPv4 packet to continue.

The kernel also replies to requests for the configured guest IP. Inspect the cache with:

```text
arp
```

A typical entry is printed in ASCII so it works in VGA text mode:

```text
10.0.2.2 -> 52:54:00:12:34:56
```

## IPv4

`src/kernel/drivers/ip.c` creates a 20-byte IPv4 header without options. It sets the protocol, source and destination addresses, total length, packet ID, TTL, and Internet checksum.

Received packets are rejected when they have an invalid version, an undersized header, an inconsistent total length, or a bad checksum. Registered protocol handlers receive the source address, payload pointer, and payload length.

`ip_send()` performs this sequence:

```text
build IPv4 header
  -> resolve destination MAC with ARP
  -> build Ethernet frame
  -> submit frame to RTL8139
```

IPv4 fragmentation, routing, TCP, UDP, and socket abstractions are outside the current implementation.

## ICMP and ping

`src/kernel/drivers/icmp.c` handles ICMP type 8 echo requests and type 0 echo replies. It validates the ICMP checksum before dispatch. Echo requests are reflected with the same identifier, sequence number, and payload.

The shell command sends four 64-byte ICMP messages:

```text
ping 10.0.2.2
```

Each request waits while the kernel polls Ethernet frames. A successful reply prints the source IP, sequence number, and TTL. ARP or transmit failure produces an error instead of waiting indefinitely.

## Initialization and runtime flow

The relevant `kernel_main()` order is:

```text
pci_init()
  -> find RTL8139 and enable PCI bus mastering
  -> rtl8139_init()
  -> arp_init()
  -> eth_init()
  -> ip_init()
  -> shell loop calls eth_dispatch()
```

The receive path is:

```text
RTL8139 RX ring
  -> rtl8139_poll()
  -> Ethernet destination and EtherType checks
  -> ARP or IPv4 handler
  -> ICMP handler
```

## Troubleshooting

### `No ethernet controller found`

Check that QEMU includes `-nic user,model=rtl8139`. Run `pci` and inspect the printed class and vendor/device values.

### `BAR0 is 0` or the NIC is not ready

The current driver expects an I/O BAR. A memory-mapped BAR requires a future MMIO mapping path.

### `DMA buffer is not mapped`

The paging system could not translate an RX or TX buffer. Inspect memory layout and ensure the buffer remains in mapped physical memory.

### `ARP resolve failed`

Run `nicinfo`, confirm the MAC and command register look valid, then retry `ping 10.0.2.2`. Check that QEMU is using the RTL8139 model and that the guest address remains `10.0.2.15`.

### No ICMP reply

First check whether `arp` gained a `10.0.2.2` entry. If it did, the Ethernet and ARP path works; investigate IPv4 or ICMP dispatch next.
