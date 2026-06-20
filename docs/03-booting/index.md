---
layout: default
title: booting
---

# booting

## grub

kernel dikemas sebagai iso dengan grub bootloader. header multiboot:

```asm
dd 0x1BADB002    ; magic number
dd 0x00          ; flags
dd -(0x1BADB002) ; checksum
```

grub load kernel.bin di physical address 1mb (0x100000).

## entry point (entry.asm)

```asm
_start:
    cli
    mov esp, stack_top     ; stack 64kb di bss
    call kernel_main
.hang:
    hlt
    jmp .hang

section .bss
resb 65536
stack_top:
```

kernel_main gak pernah return. kalo return, cpu hlt.

## gdt

gdt di-overwrite dari grub dengan yang baru:

| selector | segment | ring |
|----------|---------|------|
| 0x00 | null | - |
| 0x08 | kernel code | 0 |
| 0x10 | kernel data | 0 |
| 0x18 | user code | 3 |
| 0x20 | user data | 3 |
| 0x28 | tss | 0 |

tss nyimpen kernel stack (esp0/ss0) untuk ring 3 -> ring 0 transition.

## idt

256 entries. 32 isr (cpu exception), 16 irq (hardware), int 0x80.

semua isr pake macro yang ujungnya lompat ke `isr_common_stub`. handler nyimpen semua register via pusha, panggil `interrupt_handler()`, lalu iret.
