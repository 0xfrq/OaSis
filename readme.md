# OaSis

**Sistem operasi sederhana buat belajar.**

---

## Fitur yang Udah Ada

### Booting & Kernel
- **Multiboot-compliant bootloader** - boot lewat GRUB atau langsung pake QEMU
- **32-bit protected mode** - jalan di x86 (i386)
- **GDT & IDT** - segmentation sama interrupt handling udah beres

### Interrupt & Hardware
- **PIC (Programmable Interrupt Controller)** - ngatur IRQ dari hardware
- **Timer (PIT)** - tick 100Hz buat scheduling
- **Keyboard driver** - baca input dari keyboard PS/2
  - Support **Shift** (kiri & kanan) buat huruf kapital dan simbol
  - Layout US QWERTY lengkap (`! @ # $ % ^ & * ( ) _ + { } : " < > ? ~ |` dst)
  - Support **Ctrl** combinations (Ctrl+S, Ctrl+X, Ctrl+Z)
  - Support **arrow keys**, Home, End, PageUp/Down, Delete
- **VGA text mode** - output ke layar, support warna

### Memori
- **Deteksi memori (E820)** - tau berapa RAM yang tersedia
- **Physical Memory Manager (PMM)** - ngatur page frame allocation
- **Paging** - virtual memory udah enable

### Proses & Task
- **Task scheduler** - preemptive multitasking pake timer interrupt
- **Fork, exec, wait** - basic process management
- **File descriptor per-proses** - tiap task punya FD table sendiri

### I/O Subsystem
- **File descriptor** - stdin, stdout, stderr, pipe, file
- **Pipe** - komunikasi antar proses (IPC)
- **Dup/Dup2** - duplicate file descriptor

### Storage & Filesystem
- **ATA/IDE driver** - baca-tulis ke hard disk via PIO
- **Block device layer** - abstraction + cache
- **OAFS (Oasis File System)** - filesystem custom, inode-based
  - Support file & directory
  - Read, write, create, delete
  - Path resolution (absolute & relative)

### Shell
- Command-line interface sederhana
- Command yang available:
  ```
  help, clear, uptime, meminfo, taskinfo
  ls, cd, pwd, mkdir, touch, rm, rmdir
  cat, write, append, edit, asm, nasm
  iotest, fdinfo, pipetest, disktest, diskinfo
  ```

### Text Editor
- **Nano-like editor** - editor teks sederhana yang bisa dipake langsung di shell
- **Arrow keys** - navigasi cursor pake panah
- **Ctrl+S** - simpan file
- **Ctrl+X** - keluar (auto-save kalo ada perubahan)
- **Scrolling** - support scroll buat file yang panjang
- **Status bar** - nunjukin nama file, baris, dan kolom
- Cara pake: `edit /path/to/file`

### Simple Assembler
- **Dua mode**: interaktif (`asm`) atau assemble file dari disk (`nasm <file.asm>`)
- **Mode interaktif (`asm`)**: tulis kode assembly langsung di shell, baris per baris, akhiri dengan `---`
- **Mode file (`nasm <file>`)**: simpan dulu kode ke file pake `edit`, lalu `nasm` bakal load file, assemble, print info, jalanin
- **Workflow lengkap (write → assemble → run)**:
  ```
  > edit hello.asm        # tulis kode di editor, Ctrl+S simpan, Ctrl+X keluar
  > nasm hello.asm        # assemble & jalanin, output: byte count, alamat, hex dump, eksekusi
  ```
- **Instruksi yang didukung**:
  - Arithmetic: `mov`, `add`, `sub`, `cmp`, `xor`, `and`, `or`, `inc`, `dec`
  - Stack: `push`, `pop`, `pusha`, `popa`
  - Control flow: `jmp`, `je/jz`, `jne/jnz`, `jg`, `jl`, `jge`, `jle`, `call`, `ret`
  - System: `int`, `nop`, `hlt`, `sti`, `cli`
  - Data: `db 'string'` atau `db 0x41`
- **Register**: `eax`, `ecx`, `edx`, `ebx`, `esp`, `ebp`, `esi`, `edi`
- **Label**: diakhiri `:`, mendukung forward & backward reference
- **Komentar**: diawali `;`
- **Memory addressing**: `[0xB8000]`, `[eax]`, `mov byte [addr], imm`
- **Immediate**: desimal atau hex (prefix `0x`)
- **Auto-return**: assembler otomatis sisip `ret` sebelum data block dan di akhir kode, jadi setelah eksekusi otomatis balik ke shell
- **Sandbox memory**: kode di-load ke virtual address terpisah (`0x200000`) pake `pmm_alloc_page()` + `page_map()`
- Contoh Hello World (mode interaktif, akhiri dengan `---`):
  ```asm
  mov eax, 0          ; sys_write
  mov ebx, msg        ; pointer string
  mov ecx, 13         ; panjang string
  int 0x80
  msg:
  db 'Hello World!'
  ---
  ```
- Contoh Hello World (mode file):
  ```
  > edit hello.asm
  # isi file:
  mov eax, 0
  mov ebx, msg
  mov ecx, 13
  int 0x80
  msg:
  db 'Hello World!'
  # Ctrl+S, Ctrl+X
  > nasm hello.asm
  Mengassemble hello.asm...
  Kode mesin: 27 byte di alamat 0x200000
  Bytes: b8 00 00 00 00 bb ... cd 80 c3 48 65 6c 6c 6f ...
  Menjalankan...
  Hello World!
  [selesai]
  ```

### System Calls
- `write`, `read`, `open`, `close`, `seek`
- `fork`, `exec`, `wait`, `exit`
- `pipe`, `dup`, `dup2`
- `yield`, `sleep`, `getpid`, `getppid`
- `block_read`, `block_write`, `block_flush`

### Mini Libc (Standard Library)
Implementasi libc minimal untuk user-space:
- **I/O**: `printf`, `scanf`, `putchar`, `getchar`, `puts`, `gets`
- **File**: `open`, `close`, `read`, `write`
- **Formatting**: Mendukung spesifikator `%d`, `%i`, `%u`, `%x`, `%X`, `%o`, `%s`, `%c`, `%p`, `%%`
- **Fitur Spesial**: Field width (e.g., `%10d`), left alignment (`-`), zero padding (`0`)
- **Catatan**: Saat ini `printf` unbuffered dan belum mendukung floating point atau presisi.

---

## Struktur Proyek

```
OaSis/
├── src/
│   ├── boot/
│   │   ├── entry.asm          # entry point, multiboot header
│   │   └── linker.ld          # linker script
│   └── kernel/
│       ├── core/              # inti kernel
│       │   ├── kernel.c       # main loop + shell
│       │   ├── memory.c       # deteksi memori (E820)
│       │   ├── paging.c       # virtual memory
│       │   ├── pmm.c          # physical memory manager
│       │   └── vga.c          # VGA text mode driver
│       ├── drivers/           # hardware drivers
│       │   ├── ata.c          # ATA/IDE disk
│       │   ├── block.c        # block device + cache
│       │   ├── idt.c          # interrupt descriptor table
│       │   ├── io.c           # port I/O (inb/outb)
│       │   ├── keyboard.c     # PS/2 keyboard
│       │   ├── pic.c          # programmable interrupt controller
│       │   └── timer.c        # PIT timer
│       ├── fs/                # filesystem
│       │   ├── fd.c           # file descriptor layer
│       │   └── vfs.c          # OAFS implementation
│       ├── lib/               # utility
│       │   └── string.c       # string functions
│       ├── syscall/           # system call
│       │   ├── syscall.c      # syscall dispatcher
│       │   └── interrupt.asm  # int 0x80 wrapper
│       └── tasks/             # demo tasks
│           ├── task.c         # task manager / scheduler
│           ├── tasks_demo.c   # demo: idle & worker
│           ├── tasks_io.c     # demo: I/O subsystem
│           └── tasks_11.c     # demo: block device
│       └── apps/              # aplikasi user
│           ├── editor.c       # text editor (nano-like)
│           └── asm.c          # assembler x86 interaktif
├── include/                   # semua header file (.h)
├── iso/boot/grub/             # GRUB config buat ISO
├── .gitignore                 # git ignore rules
├── Makefile
├── readme.md
└── disk.img                   # disk image buat filesystem (gak masuk git)
```

---

## Cara Build & Jalanin

### Prasyarat
- GCC (cross-compiler atau native, yang penting support `-m32`)
- NASM (assembler)
- GRUB + `grub-mkrescue` (buat bikin ISO)
- QEMU (`qemu-system-i386`)
- `xorriso` (dependency grub-mkrescue)

### Build
```bash
make            # compile + bikin ISO
make clean      # bersihin semua hasil build
```

### Jalanin
```bash
make run        # langsung boot kernel di QEMU
```

### Bikin ISO (opsional)
```bash
make iso        # bikin oasis.iso yang bisa di-burn atau di-boot via GRUB
```

---

## Catatan Teknis

### Arsitektur
- **32-bit x86** (i386)
- **Monolithic kernel** - semua jalan di ring 0
- **Preemptive multitasking** - timer interrupt bikin task switch otomatis
- **No user mode** - semua task jalan di kernel mode (buat sekarang)

### Filesystem (OAFS)
- Disk layout: `[Superblock] [Inode Table] [Data Blocks]`
- Max 1024 inode
- 12 direct block pointer per inode (max ~6KB per file)
- Block size: 512 byte

### System Call
- Via `int 0x80` - eax = syscall number, ebx/ecx/edx = arguments
- Return value di eax

---

## Rencana Ke Depan

### Jangka Pendek
- [ ] **User mode (ring 3)** - task jalan di user space, bukan kernel mode
- [ ] **ELF loader** - bisa load dan execute binary ELF dari filesystem
- [ ] **Indirect block** - support file lebih gede dari 6KB di OAFS
- [ ] **Heap allocator** - `kmalloc`/`kfree` buat dynamic memory di kernel
- [ ] **Better scheduler** - priority-based atau CFS-like scheduling

### Jangka Menengah
- [ ] **Networking** - minimal UDP/TCP stack
- [ ] **VFS abstraction** - support multiple filesystem (FAT32, ext2)
- [ ] **Framebuffer graphics** - keluar dari text mode, pake pixel
- [ ] **Sound** - PC speaker atau basic audio driver
- [ ] **Serial port** - output ke serial console buat debugging

### Jangka Panjang
- [ ] **SMP** - support multiple CPU core
- [ ] **USB driver** - support USB keyboard/mouse/storage
- [ ] **Shell scripting** - pipe, redirect, environment variable
- [ ] **Package manager** - install program dari repository
- [ ] **GUI** - simple windowing system

---

## Kontribusi

Ini proyek belajar, jadi feel free buat fork, modif, atau experiment. Kalo mau contribute, tinggal bikin branch, commit, dan PR. Gak ada aturan baku soal commit message - yang penting jelas.

---

## Lisensi

Bebas dipake buat belajar. No warranty - ini OS edukasi, bukan production-ready.

---

**Dibikin dengan kopi dan rasa penasaran.**