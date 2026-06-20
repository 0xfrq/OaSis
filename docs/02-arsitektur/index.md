---
layout: default
title: arsitektur
---

# arsitektur

oasis pake desain monolithic kernel -- semua layanan (filesystem, driver, syscall) jalan di kernel mode.

## layer

```
+------------------------------------------+
|  shell (kernel_main loop)                |
+------------------------------------------+
|  syscall layer (23 syscalls via int 0x80)|
+------------------------------------------+
|  filesystem (oafs + fd layer)            |
+------------------------------------------+
|  task scheduler + process isolation      |
+------------------------------------------+
|  memory management (pmm + paging + heap) |
+------------------------------------------+
|  device drivers (keyboard, timer, ata)   |
+------------------------------------------+
|  boot + gdt + idt + interrupts           |
+------------------------------------------+
```

## alur boot

```
grub -> entry.asm -> kernel_main()
  -> gdt_init() -> idt_init() -> pic_init()
  -> timer_init(100hz) -> keyboard_init()
  -> memory_init (e820) -> pmm_init -> paging_init
  -> task_init -> fd_init -> block_init
  -> vfs_init (oafs) -> syscall_init
  -> shell loop
```
