---
layout: default
title: Build OaSis
description: Install the toolchain, build the kernel, create an ISO, and launch QEMU.
content_type: how-to
audience: contributors and first-time users
goal: Build and boot OaSis with the same configuration used by development.
---

# Build OaSis

This page covers the reproducible build and boot workflow. Use [Testing](testing/) for host-side protocol checks and the complete networking smoke test.

## Prerequisites

On Debian or Ubuntu, install:

```bash
sudo apt install gcc-multilib nasm grub-pc-bin grub2-common xorriso qemu-system-x86
```

The build targets a freestanding 32-bit i386 kernel. `gcc-multilib` provides the 32-bit compiler support required by `-m32`.

The default `make run` target also expects `disk.img` in the repository root. The image is used as the IDE disk for OAFS tests and filesystem persistence.

## Build targets

| command | Result |
| --- | --- |
| `make` | Compile the kernel and create `oasis.iso`. |
| `make clean` | Remove object files, `kernel.bin`, and `oasis.iso`. |
| `make run` | Boot `kernel.bin` directly in QEMU with the disk and RTL8139. |

Build from the repository root:

```bash
make clean
make
```

The C compiler uses freestanding 32-bit flags. NASM produces ELF32 objects, and `ld` links the kernel with `src/boot/linker.ld` at physical address 1 MiB.

## Run QEMU

The Makefile launches QEMU with:

```bash
qemu-system-i386 \
  -kernel kernel.bin \
  -drive id=disk0,file=disk.img,format=raw,if=none \
  -device ide-hd,drive=disk0,bus=ide.0 \
  -nic user,model=rtl8139 \
  -m 512M
```

The direct-kernel target is convenient for development. `oasis.iso` is generated for GRUB or ISO boot testing, but the `run` target does not boot the ISO.

## Check the boot sequence

The kernel prints initialization messages to VGA text mode. Once the shell prompt appears, run:

```text
help
meminfo
uptime
```

For networking, continue with:

```text
pci
nicinfo
arp
ping 10.0.2.2
arp
```

See [Networking internals](../05-driver/networking/) for the device and protocol path.

## Debugging

Use `dmesg` in the OaSis shell to inspect the circular kernel log:

```text
dmesg
```

Exceptions are recorded automatically. The kernel also writes compact exception details to QEMU's debug port `0xE9`; a custom QEMU invocation can capture it with a debug console:

```bash
qemu-system-i386 \
  -kernel kernel.bin \
  -debugcon file:debug.log \
  -global isa-debugcon.iobase=0xe9
```

The main shell remains on VGA, so do not use `-nographic` when you need to interact with the text console.

## Common build problems

- if `gcc -m32` fails, install `gcc-multilib`.
- if `grub-mkrescue` fails, install `grub-pc-bin`, `grub2-common`, and `xorriso`.
- if QEMU cannot open `disk.img`, create or copy the project disk image before running `make run`.
- if networking reports no controller, confirm the QEMU option contains `model=rtl8139`.
