# 03. booting

dokumentasi ini ngebahas gimana OaSis boot dari power-on sampe kernel mulai jalan.

## daftar isi

- [overview](#overview)
- [multiboot specification](#multiboot-specification)
- [entry point](#entry-point)
- [inisialisasi awal](#inisialisasi-awal)
- [transisi ke kernel](#transisi-ke-kernel)

---

## overview

proses booting OaSis:

```
power on
  ↓
bios/firmware
  ↓
bootloader (grub)
  ↓
kernel (entry.asm)
  ↓
kernel main (main.c)
  ↓
shell ready
```

**catatan:** OaSis pake grub sebagai bootloader karena grub udah handle banyak hal kompleks (load kernel, setup multiboot, dll).

## multiboot specification

OaSis mengikuti **multiboot specification**, yang artinya:

- kernel bisa di-load oleh bootloader yang compliant (grub, syslinux, dll)
- kernel harus punya multiboot header di awal file
- bootloader bakal pass info ke kernel via multiboot info structure

### multiboot header

file `src/boot/entry.asm` punya multiboot header:

```asm
section .multiboot
align 4
    dd 0x1BADB002        ; magic number
    dd 0x00000003        ; flags (align modules, memory info)
    dd -(0x1BADB002 + 3) ; checksum
```

**magic number:** `0x1BADB002` - tanda ini multiboot kernel

**flags:**
- bit 0: align modules on page boundary
- bit 1: provide memory map

## entry point

entry point OaSis ada di `src/boot/entry.asm`:

```asm
section .text
global _start
extern main

_start:
    ; setup stack
    mov esp, stack_top
    
    ; call kernel main
    call main
    
    ; infinite loop (shouldn't reach here)
    jmp $
```

### apa yang dilakuin entry point?

1. **setup stack pointer**
   - allocates stack space (16 KB)
   - set `esp` ke top of stack

2. **call kernel main**
   - lompat ke `main()` di `main.c`
   - kernel mulai inisialisasi

3. **infinite loop**
   - fallback kalo main return (shouldn't happen)

## inisialisasi awal

setelah entry point, kernel mulai inisialisasi di `src/kernel/main.c`:

```c
void main(void) {
    // 1. clear screen
    vga_clear();
    
    // 2. init gdt
    gdt_init();
    
    // 3. init idt
    idt_init();
    
    // 4. init pic
    pic_init();
    
    // 5. init timer
    timer_init();
    
    // 6. init keyboard
    keyboard_init();
    
    // 7. enable interrupts
    asm volatile("sti");
    
    // 8. init memory
    pmm_init();
    
    // 9. init vfs
    vfs_init();
    
    // 10. start shell
    shell_run();
}
```

### urutan inisialisasi

**kenapa urutan ini penting?**

1. **vga dulu** - biar bisa print debug message
2. **gdt** - segmentation harus di-setup sebelum protected mode
3. **idt** - interrupt descriptor table buat handle interrupt
4. **pic** - programmable interrupt controller
5. **timer** - butuh idt dan pic
6. **keyboard** - butuh idt dan pic
7. **sti** - enable interrupt setelah semua siap
8. **memory** - butuh timer buat tracking
9. **vfs** - butuh memory management
10. **shell** - terakhir, setelah semua subsystem ready

## transisi ke kernel

setelah semua inisialisasi selesai, kernel spawn shell:

```c
void shell_run(void) {
    while (1) {
        shell_print_prompt();
        char *input = shell_read_line();
        shell_execute(input);
    }
}
```

shell jalan di infinite loop, nunggu input dari user dan execute command.

detail shell ada di [07-shell](../07-shell/readme.md)

---

selanjutnya: [kernel →](../04-kernel/readme.md)
