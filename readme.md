# OaSis OS

OaSis is an educational 32-bit x86 operating system built from scratch in C and assembly. It boots with GRUB, runs a protected-mode kernel, supports ring 3 user programs, and includes an OAFS filesystem, shell, assembler, and subset C compiler.

Documentation: [oasis.fariqdoing.tech](https://oasis.fariqdoing.tech)

## Current status

| Area | Status |
| --- | --- |
| i386 boot and protected mode | Implemented: Multiboot entry, GDT, IDT, PIC, and TSS |
| Memory management | Implemented: E820 detection, bitmap PMM, paging, per-task CR3, and kernel heap |
| Tasks and user mode | Implemented: round-robin tasks, ring 3 entry, process isolation, and 23 system calls |
| OAFS filesystem | Implemented: inode filesystem, directories, indirect blocks, file descriptors, and block cache |
| Shell and editor | Implemented: filesystem commands, diagnostics, assembler, compiler, and nano-like editor |
| `occ` compiler | Implemented subset: `int`, `char`, functions, parameters, arrays, expressions, loops, conditionals, `printf`, and allocation |
| Built-in assembler | Implemented subset: x86 instructions, labels, `times`, data directives, and external symbols |
| Hardware drivers | Implemented: VGA text mode, PS/2 keyboard, PIT, ATA PIO, PCI, and RTL8139 |
| Network stack | Working in QEMU: Ethernet, ARP, IPv4, ICMP echo, `ping`, and ARP cache |
| Host protocol tests | Implemented: checksum, IPv4 dispatch, malformed checksum, and length validation tests |
| Graphical interface | Not implemented yet; the current UI is VGA text mode |

## Quick start

Install the toolchain on Debian or Ubuntu:

```bash
sudo apt install gcc-multilib nasm grub-pc-bin grub2-common xorriso qemu-system-x86
```

Build the kernel and ISO:

```bash
make clean
make
```

Run the kernel with the disk image and QEMU's RTL8139 user-mode network:

```bash
make run
```

The `run` target uses:

```text
-kernel kernel.bin
-drive ... disk.img ...
-nic user,model=rtl8139
-m 512M
```

## Verify networking

After the OaSis shell appears, run these commands:

```text
pci
nicinfo
arp
ping 10.0.2.2
arp
```

A successful run discovers the RTL8139, prints a MAC address, resolves `10.0.2.2` through ARP, and receives ICMP echo replies. The default guest configuration is defined in `include/netcfg.h`:

```text
Guest IP: 10.0.2.15
Gateway:  10.0.2.1
Netmask:  255.255.255.0
DNS:      10.0.2.3
```

## Networking architecture

The current network path is intentionally small and QEMU-focused:

```text
ping <ip>
  -> ip_send()
  -> arp_resolve()
  -> Ethernet broadcast ARP request
  -> RTL8139 transmit descriptor
  -> QEMU user-mode network
  -> RTL8139 receive ring
  -> eth_dispatch()
  -> arp_handle_packet()
  -> ARP cache
  -> IPv4 packet
  -> ICMP echo reply
```

The stack supports Ethernet type `0x0806` (ARP) and `0x0800` (IPv4). IPv4 currently dispatches ICMP protocol `1`. The receive path is polled from the shell loop and while `ping` waits for replies.

Current network limitations:

- The default address configuration is static rather than DHCP-managed.
- The driver targets QEMU's RTL8139 model and uses polling rather than NIC interrupts.
- IPv4 fragmentation, routing tables, TCP, UDP, DHCP, DNS resolution, sockets, and network syscalls are not implemented.
- The network stack is currently kernel-owned; `occ` programs do not have a network API.

## Host-side protocol test

`test_network.c` tests protocol logic without hardware. Compile and run it from the repository root:

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

Expected output includes:

```text
ok: RFC 1071 checksum
ok: odd-length checksum
ok: valid IPv4 dispatch
ok: bad checksum rejected
ok: truncated total length rejected
```

These tests do not emulate PCI, DMA, QEMU, or port I/O. Use the QEMU workflow for hardware integration.

## Repository map

```text
src/boot/                 Multiboot entry point and linker script
src/kernel/core/          Kernel, memory, paging, GDT, and VGA
src/kernel/drivers/       ATA, PCI, RTL8139, Ethernet, ARP, IP, ICMP, input, and timers
src/kernel/fs/             OAFS VFS and file descriptors
src/kernel/lib/            String library, heap, logging, compiler, and user library
src/kernel/syscall/        System call dispatcher and assembly entry
src/kernel/tasks/          Scheduler and user-task support
src/kernel/apps/           Text editor and built-in assembler
include/                   Kernel headers
iso/                       GRUB files and ISO staging area
docs/                      Jekyll documentation site
test_network.c             Host-side network protocol tests
Makefile                   Build and QEMU targets
```

## GUI roadmap

OaSis does not have a graphical desktop yet. VGA output is still an 80 x 25 text console. The planned path is:

1. Read a Multiboot framebuffer and add pixel primitives.
2. Render a bitmap font and move the shell to a graphical terminal.
3. Add PS/2 mouse input and an event queue.
4. Add a compositor with a terminal and editor window.
5. Add GUI syscalls and a small user-space GUI library.

See the [GUI roadmap](docs/11-gui/) and [documentation site](https://oasis.fariqdoing.tech) for the longer plan.

## More documentation

- [Architecture](docs/02-arsitektur/)
- [Boot sequence](docs/03-booting/)
- [Networking internals](docs/05-driver/networking/)
- [Shell commands](docs/07-shell/)
- [Build and test guide](docs/09-build/)
- [GUI roadmap](docs/11-gui/)
- [Changelog](docs/10-changelog/)

## License

OaSis is released under the MIT license.
