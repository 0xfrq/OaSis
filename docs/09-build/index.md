---
layout: default
title: Build
---

# 09. Build & Development

## Prerequisites

```
sudo apt install gcc nasm grub2-common xorriso qemu-system-x86
```

## Make Targets

| Target | Description |
|--------|-------------|
| `make` | Build kernel.bin + oasis.iso |
| `make clean` | Remove all build artifacts |
| `make run` | Boot in QEMU |

## Build Details

### Kernel Image Build

1. C source files compiled with flags:
   ```
   -m32 -nostdlib -fno-builtin -fno-stack-protector
   -ffreestanding -fno-pie -fno-pic -Wall -Wextra
   ```

2. Assembly files assembled with NASM (`-f elf32`).

3. Linked with `ld -m elf_i386 -T src/boot/linker.ld -no-pie`.

4. Linker sets entry at 1MB physical, sections: .multiboot, .text, .rodata, .data, .bss.

5. kernel.bin copied to ISO image with GRUB bootloader.

### Filesystem Disk

The disk image (`disk.img`) contains the OAFS filesystem:
- Block device backed by a file.
- On first boot, VFS formats the disk with root directory + /home, /bin, /tmp.
- Superblock magic determines whether to format or load.

## Debugging

### QEMU Debug Port
The kernel outputs debug messages to QEMU's debug console port (0xE9):
```
outb(0xE9, (uint8_t)*s++);
```

Capture with: `qemu -debugcon stdio`

### Exception Logging
All exceptions are logged to the kernel log buffer:
```
dmesg    # view kernel log
```

The log includes interrupt number, error code, CR2 (page fault address), and EIP.

### Common Issues

| Symptom | Likely Cause |
|---------|-------------|
| Triple fault (QEMU restart) | Corrupt IDT/GDT, invalid stack pointer |
| Page fault at 0x40000000 | CODE_VIRT not mapped (asm_assemble) |
| Page fault at 0x40001000 | Code/data crosses page boundary |
| GPF err_code=0x20 | Attempting to load ring 3 SS from ring 0 |
| GPF err_code=0x05 (EXT) | Interrupt gate DPL too restrictive for ring 3 |
| Empty file listing | OAFS reformatted, data lost |
