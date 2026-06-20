---
layout: default
title: booting
---

# booting

## multiboot header

kernel dimulai dengan header multiboot di section `.multiboot`:

```asm
SECTION .multiboot
 align 4
 dd 0x1BADB002 ; magic number
 dd 0x00 ; flags (none, aout kludge tidak dipake)
 dd -(0x1BADB002) ; checksum: magic + flags + checksum = 0
```

grub membaca header ini dan load kernel.bin di physical address 1mb (0x100000). kernel binary adalah elf32, grub parse elf header dan load segments sesuai.

## entry point (src/boot/entry.asm)

setelah grub selesai, cpu mulai eksekusi di `_start`:

```asm
SECTION .text
GLOBAL _start
EXTERN kernel_main

_start:
 cli ; matiin interrupt dulu
 mov esp, stack_top ; set stack pointer ke stack 64kb
 call kernel_main ; panggil kernel c

.hang:
 hlt ; kalo kernel_main return, halt
 jmp .hang

section .bss
align 16
resb 65536 ; 64kb stack
stack_top:
```

stack_top adalah symbol yang di-expose ke c via `extern uint32_t stack_top;`, dipake sama gdt.c buat set esp0 di tss.

## kernel_main initialization sequence

### urutan inisialisasi

```
1. vga_clear() -- bersihin layar 80x25
2. boot_screen() -- tampilkan "oasis os" + border + "booting..."
3. gdt_init() -- setup 6 gdt entries (ring0 + ring3 + tss)
4. idt_init() -- 256 idt entries (32 isr + 16 irq + int 0x80)
5. pic_init() -- remap irq ke interrupt 32-47
6. timer_init(100) -- pit channel 0 di 100hz
7. keyboard_init() -- ps/2 keyboard, flush buffer
8. boot_progress() -- update loading dots
9. sti -- enable interrupts
10. memory_init() -- e820 memory detection
11. pmm_init() -- physical memory manager (bitmap)
12. paging_init() -- page tables + identity map + higher-half
13. paging_enable() -- set cr0.pg = 1
14. boot_progress()
15. task_init() -- init task array + scheduler
16. fd_init() -- file descriptor layer
17. block_init() -- block device cache
18. vfs_init() -- oafs filesystem (format kalo belum ada)
19. syscall_init() -- set idt entry 128 (0x80) dengan dpl=3
20. task_create() x3 -- idle, worker, block_test
21. boot_progress() -- dots selesai
22. vga_clear() -- bersihin layar
23. shell prompt -- "=== oasis os ===" + "type help" + prompt
```

### gdt_init

setup gdt entries dan panggil lgdt. setelah lgdt, reload segment registers:
- ds/es/fs/gs = 0x10 (kernel data)
- cs = 0x08 (kernel code, via far jump)
- ltrw: load tss selector (0x28) ke task register

tss diisi:
- esp0 = stack_top (0x29E840)
- ss0 = 0x10

nilai esp0 dihitung dari `(uint32_t)&stack_top`. stack_top adalah label di entry.asm, alamatnya sekitar 0x29E840 (setelah kernel bss ~2.9mb).

### idt_init

set 32 isr + 16 irq + int 0x80 dengan:
- handler: address masing-masing isr/irq wrapper (isr_0..isr_31, irq_0..irq_15, int_80_wrapper)
- selector: 0x08 (kernel code)
- type_attr: 0x8E untuk isr/irq (present, ring0, interrupt gate), 0xEF untuk int 0x80 (present, ring3, interrupt gate)

kenapa int 0x80 pake dpl=3? karena kalo dpl=0, dari ring 3 gabisa panggil int 0x80 -> general protection fault.

### pic_init

pic master dan slave di-remap:
- master: icw2 = 32 (irq 0 -> int 32)
- slave: icw2 = 40 (irq 8 -> int 40)
- semua irq di-mask (0xFF) dulu, kemudian irq 0 (timer) dan irq 1 (keyboard) di-unmask

### timer_init

pit channel 0 di-set ke mode 2 (rate generator):
- divisor = 1193182 / 100 = 11931
- kirim divisor via port 0x40 (low byte dulu, high byte)

handler timer (irq_0) bakal dipanggil 100x per detik.

### sti

setelah semua inisialisasi hardware, enable interrupts. ini penting karena keyboard gak bakal work tanpa interrupt.

### memory_init

panggil int 0x15 e820 buat dapetin memory map. hasilnya disimpan di `e820_map`. map ini dipake buat itung total usable memory.

### pmm_init

bitmap 1mb (8m bit) di-reset. kernel area (0-1mb + kernel binary sampe _end) di-mark sebagai used.

### paging_init

page tables dibuat:
1. pde[0]: identity map 0-4mb (kernel code, vga buffer, dll)
2. pde[0xC00..0xC03]: higher-half mapping 0xC0000000-0xC0400000 (akses kernel dari high address)

### paging_enable

```c
uint32_t pd_phys = (uint32_t)kernel_page_dir; // physical address
asm volatile("mov %0, %%cr3" : : "r"(pd_phys));
uint32_t cr0;
asm volatile("mov %%cr0, %0" : "=r"(cr0));
cr0 |= 0x80000000; // PG bit
asm volatile("mov %0, %%cr0" : : "r"(cr0));
```

setelah paging_enable, semua akses memory pake page tables. kernel masih bisa akses semuanya karena identity map.

### vfs_init

coba load superblock dari disk. kalo magic cocok, load inode table dan rebuild block bitmap. kalo gak cocok, format filesystem baru (bikin root + /home + /bin + /tmp).

### syscall_init

satu baris:
```c
idt_set_entry(0x80, (uint32_t)&int_80_wrapper, 0x08, 0xEF);
```

0xEF = present | ring3 | interrupt gate. ini yang bikin int 0x80 bisa dipanggil dari user mode.
