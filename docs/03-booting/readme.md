---
layout: default
title: Booting
---

# 03. Booting

## Boot Sequence

### 1. GRUB (Multiboot)
OaSis uses GRUB as the bootloader via the Multiboot specification. The kernel header includes:
```
dd 0x1BADB002    ; Magic number
dd 0x00           ; Flags (none)
dd -(0x1BADB002) ; Checksum
```

GRUB loads the kernel ELF binary at physical address 1MB (0x100000).

### 2. Entry Point (`src/boot/entry.asm`)

The entry point sets up the minimal execution environment:
1. Disable interrupts (`cli`).
2. Set stack pointer to `stack_top` (64KB BSS stack).
3. Call `kernel_main()` — the C entry point.
4. If `kernel_main` returns, halt indefinitely.

### 3. Kernel Initialization Sequence

```
kernel_main():
  vga_clear()         # Clear screen
  gdt_init()          # Setup GDT with ring 0 and ring 3 segments + TSS
  idt_init()          # Setup IDT (32 ISR + 16 IRQ + int 0x80)
  pic_init()          # Programmable Interrupt Controller
  timer_init(100)     # PIT timer at 100Hz
  keyboard_init()     # PS/2 keyboard
  sti                 # Enable interrupts
  memory_init()       # E820 memory detection
  pmm_init()          # Physical memory manager
  paging_init()       # Page tables
  paging_enable()     # Enable paging (CR0.PG = 1)
  task_init()         # Initialize scheduler
  fd_init()           # File descriptor layer
  block_init()        # Block device cache
  vfs_init()          # OAFS filesystem (load or format)
  syscall_init()      # int 0x80 gate
  task_create(...)    # Create kernel tasks
  Shell loop          # Command-line interface
```

### 4. GDT Layout

| Selector | Segment | Ring | Purpose |
|----------|---------|------|---------|
| 0x00 | NULL | — | Required null descriptor |
| 0x08 | Kernel Code | 0 | .text execution |
| 0x10 | Kernel Data | 0 | .data, .bss access, stack |
| 0x18 | User Code | 3 | Ring 3 code execution |
| 0x20 | User Data | 3 | Ring 3 data access |
| 0x28 | TSS | 0 | Task State Segment |

### 5. TSS (Task State Segment)

The TSS is essential for ring 3 -> ring 0 transitions:
- **ESP0**: Kernel stack address (stack_top).
- **SS0**: Kernel data segment (0x10).
- On `int 0x80` from user mode, the CPU reads ESP0/SS0 from TSS to find the kernel stack.

Without a proper TSS, any interrupt from ring 3 causes a triple fault.
