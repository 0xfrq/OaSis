---
layout: default
title: booting
---

# booting

## multiboot header

Kernel dimulai dengan header multiboot di section `.multiboot`:

```asm
SECTION .multiboot
 align 4
 dd 0x1BADB002 ; magic number
 dd 0x00 ; flags (none, aout kludge tidak dipakai)
 dd -(0x1BADB002) ; checksum: magic + flags + checksum = 0
```

GRUB membaca header ini dan me-load kernel.bin di physical address 1MB (0x100000). Kernel binary adalah ELF32, GRUB parse ELF header dan load segments sesuai.

## entry point (src/boot/entry.asm)

Setelah GRUB selesai, CPU mulai eksekusi di `_start`:

```asm
SECTION .text
GLOBAL _start
EXTERN kernel_main

_start:
 cli ; matikan interrupt dulu
 mov esp, stack_top ; set stack pointer ke stack 64KB
 call kernel_main ; panggil kernel C

.hang:
 hlt ; kalau kernel_main return, halt
 jmp .hang

section .bss
align 16
resb 65536 ; 64KB stack
stack_top:
```

`stack_top` adalah symbol yang di-expose ke C via `extern uint32_t stack_top;`, dipakai oleh `gdt.c` untuk set ESP0 di TSS.

## kernel_main initialization sequence

### urutan inisialisasi

```
 1. vga_clear() -- bersihkan layar 80x25
 2. boot_screen() -- tampilkan "oasis os" + border + "booting..."
 3. gdt_init() -- setup 6 GDT entries (ring0 + ring3 + TSS)
 4. idt_init() -- 256 IDT entries (32 ISR + 16 IRQ + int 0x80)
 5. pic_init() -- remap IRQ ke interrupt 32-47
 6. timer_init(100) -- PIT channel 0 di 100Hz
 7. keyboard_init() -- PS/2 keyboard, flush buffer
 8. boot_progress() -- update loading dots
 9. sti -- enable interrupts
10. memory_init() -- E820 memory detection
11. pmm_init() -- physical memory manager (bitmap)
12. paging_init() -- page tables + identity map + higher-half
13. paging_enable() -- set CR0.PG = 1
14. boot_progress()
15. task_init() -- init task array + scheduler
16. fd_init() -- file descriptor layer
17. block_init() -- block device cache
18. vfs_init() -- OAFS filesystem (format kalau belum ada)
19. syscall_init() -- set IDT entry 128 (0x80) dengan DPL=3
20. task_create() x3 -- idle, worker, block_test
21. boot_progress() -- dots selesai
22. vga_clear() -- bersihkan layar
23. shell prompt -- "=== oasis os ===" + "type help" + prompt
```

### gdt_init

Setup GDT entries dan panggil `lgdt`. Setelah lgdt, reload segment registers:
- DS/ES/FS/GS = 0x10 (kernel data)
- CS = 0x08 (kernel code, via far jump)
- LTRW: load TSS selector (0x28) ke task register

TSS diisi:
- ESP0 = stack_top (0x29E840)
- SS0 = 0x10

Nilai ESP0 dihitung dari `(uint32_t)&stack_top`. `stack_top` adalah label di entry.asm, alamatnya sekitar 0x29E840 (setelah kernel BSS ~2.9MB).

### idt_init

Set 32 ISR + 16 IRQ + int 0x80 dengan:
- handler: address masing-masing ISR/IRQ wrapper (isr_0..isr_31, irq_0..irq_15, int_80_wrapper)
- selector: 0x08 (kernel code)
- type_attr: 0x8E untuk ISR/IRQ (present, ring0, interrupt gate), 0xEF untuk int 0x80 (present, ring3, interrupt gate)

Kenapa int 0x80 pakai DPL=3? Karena kalau DPL=0, dari ring 3 tidak bisa memanggil int 0x80 → general protection fault.

### pic_init

PIC master dan slave di-remap:
- master: ICW2 = 32 (IRQ 0 → int 32)
- slave: ICW2 = 40 (IRQ 8 → int 40)
- semua IRQ di-mask (0xFF) dulu, kemudian IRQ 0 (timer) dan IRQ 1 (keyboard) di-unmask

### timer_init

PIT channel 0 di-set ke mode 2 (rate generator):
- divisor = 1193182 / 100 = 11931
- kirim divisor via port 0x40 (low byte dulu, high byte)

Handler timer (irq_0) akan dipanggil 100x per detik.

### sti

Setelah semua inisialisasi hardware, enable interrupts. Ini penting karena keyboard tidak akan bekerja tanpa interrupt.

### memory_init

Panggil int 0x15 E820 untuk mendapatkan memory map. Hasilnya disimpan di `e820_map`. Map ini dipakai untuk menghitung total usable memory.

### pmm_init

Bitmap 1MB (8M bit) di-reset. Kernel area (0-1MB + kernel binary sampai _end) di-mark sebagai used.

### paging_init

Page tables dibuat:
1. PDE[0]: identity map 0-4MB (kernel code, VGA buffer, dll)
2. PDE[0xC00..0xC03]: higher-half mapping 0xC0000000-0xC0400000 (akses kernel dari high address)

### paging_enable

```c
uint32_t pd_phys = (uint32_t)kernel_page_dir; // physical address
asm volatile("mov %0, %%cr3" : : "r"(pd_phys));
uint32_t cr0;
asm volatile("mov %%cr0, %0" : "=r"(cr0));
cr0 |= 0x80000000; // PG bit
asm volatile("mov %0, %%cr0" : : "r"(cr0));
```

Setelah paging_enable, semua akses memory menggunakan page tables. Kernel masih bisa mengakses semuanya karena identity map.

### vfs_init

Coba load superblock dari disk. Kalau magic cocok, load inode table dan rebuild block bitmap. Kalau tidak cocok, format filesystem baru (buat root + /home + /bin + /tmp).

### syscall_init

Satu baris:
```c
idt_set_entry(0x80, (uint32_t)&int_80_wrapper, 0x08, 0xEF);
```

0xEF = present | ring3 | interrupt gate. Ini yang membuat int 0x80 bisa dipanggil dari user mode.
