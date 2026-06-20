# OaSis -- Sistem Operasi Edukasi

<p align="center">
  <b>Sistem operasi 32-bit x86 dari nol, dibikin untuk belajar.</b><br>
  <code>Ring 3 user mode</code> * <code>23 syscalls</code> * <code>Process isolation</code> * <code>OAFS filesystem</code> * <code>occ C compiler</code>
</p>

---

## Pencapaian Terkini

| Area | Status |
|------|--------|
| **Ring 3 User Mode** | [DONE] User code jalan di ring 3, `user /file.asm` |
| **Process Isolation** | [DONE] Per-task page directory + CR3 switching, user blocked dari kernel pages |
| **System Calls** | [DONE] 23 syscalls (0-22) via `int 0x80` + eksternal symbol wrappers |
| **User Heap (brk)** | [DONE] `SYSCALL_BRK` untuk expand heap user |
| **User Space Utilities** | [DONE] `cat`, `echo`, `write` via shell built-in + user mode `.asm` |
| **Indirect Blocks** | [DONE] File > 6KB (12 direct + 128 indirect = 70KB per file) |
| **Kernel Heap** | [DONE] `kmalloc`/`kfree` / `kcalloc` / `krealloc` free-list allocator |
| **Logging** | [DONE] Circular buffer, `dmesg` command, auto-log exception, clean boot screen |
| **Filesystem (OAFS)** | [DONE] Multi-block directory (168 entries), indirect blocks, hardened, color ls |
| **occ Compiler** | [DONE] Subset C: `int`, `char`, `if/else`, `while`, `for`, function params, array subscript, `printf`, `malloc` |
| **Built-in Assembler** | [DONE] `nasm` + `times` + segment registers + label push + tab support |
| **User Space Libc** | [DONE] `usr_printf`, `usr_malloc`, `usr_gets`, `usr_puts` via syscalls |
| **Mini Libc** | [DONE] `printf`, `scanf`, `putchar`, `gets`, `sprintf`, `atoi` |
| **GDT + TSS** | [DONE] User segments (0x18/0x20) + Task State Segment |
| **Per-task CR3** | [DONE] Task switching with page directory switching |
| **Text Editor** | [DONE] Nano-like editor (`edit`) + tab support |
| **Drivers** | [DONE] Keyboard PS/2, VGA text mode, ATA/IDE, PIT timer, Block cache |

---

## Fitur Detail

### Kernel & Boot
- Multiboot-compliant (GRUB-compatible)
- 32-bit protected mode (x86 i386)
- GDT with ring 0/ring 3 segments + TSS
- IDT for 32 interrupts + 16 IRQs + int 0x80 syscall handler
- PIC (Programmable Interrupt Controller)
- VGA text mode (80x25, 16 colors)

### Manajemen Memori
- E820 memory detection
- Physical Memory Manager (PMM) -- bitmap-based page allocator
- Paging (4KB pages) -- identity map + higher-half kernel (0xC0000000+)
- **Process isolation** -- each user task gets a clone of kernel page dir
  - Kernel pages (identity map, higher-half) -> **no PTE_USER**
  - User pages (code, stack, brk) -> **with PTE_USER**
- **Per-task CR3 switching** -- task_switch() updates CR3 otomatis
- Kernel heap allocator -- free-list with splitting and coalescing
- `kmalloc`, `kfree`, `kcalloc`, `krealloc`

### User Mode (Ring 3)
- **Ring 3 execution**: user code via `user /file.asm` command
- **Process isolation**: dedicated page directory per user task
- **Syscall interface**: 23 system calls via `int 0x80`
- **Exit handling**: `SYSCALL_USER_EXIT` (21) via iret redirect -> `user_return_to_shell`
- **User heap**: `SYSCALL_BRK` (22) for dynamic memory allocation
- **User fd_table**: each user task has its own file descriptor table

### System Calls (int 0x80)
```
 0  SYSCALL_WRITE       - write to stdout
 1  SYSCALL_SLEEP       - sleep (ms)
 2  SYSCALL_YIELD       - yield task
 3  SYSCALL_EXIT        - exit process
 4  SYSCALL_GETPID      - get process ID
 5  SYSCALL_FORK        - fork process
 6  SYSCALL_EXEC        - exec program
 7  SYSCALL_WAIT        - wait for child
 8  SYSCALL_GETPPID     - get parent PID
 9  SYSCALL_OPEN        - open file
10  SYSCALL_CLOSE       - close fd
11  SYSCALL_READ        - read from fd
12  SYSCALL_WRITE_FD    - write to fd
13  SYSCALL_PIPE        - create pipe
14  SYSCALL_DUP         - dup fd
15  SYSCALL_DUP2        - dup2 fd
16  SYSCALL_SEEK        - seek in fd
17  SYSCALL_FDINFO      - debug fd table
18  SYSCALL_BLOCK_READ  - read block device
19  SYSCALL_BLOCK_WRITE - write block device
20  SYSCALL_BLOCK_FLUSH - flush block cache
21  SYSCALL_USER_EXIT   - exit user mode -> return to shell
22  SYSCALL_BRK         - expand/shrink user heap
```

### Filesystem (OAFS)
- Custom inode-based filesystem (Oasis File System)
- 1024 inodes, 8192 blocks, 512 bytes/block
- 12 direct block pointers + 1 indirect block (128 pointers) = ~70KB per file
- Support file ukuran besar via **indirect blocks**
- Directory support with path resolution (absolute + relative)
- **Hardened**: proper error messages, atomic unlink/rmdir, path validation
- Free block bitmap, inode table, superblock

### Shell (kernel_main)
- Command-line interface dengan prompt `oasis(/)`
- Command: `help`, `clear`, `uptime`, `meminfo`, `dmesg`, `syscall`
- File ops: `ls` (color-coded), `cd`, `pwd`, `mkdir`, `touch`, `rm`, `rmdir`, `cat`, `write`, `append`, `hexdump`
- Development: `edit`, `asm`, `nasm`, `occ`, `user`
- Other: `echo`, `taskinfo`, `fdinfo`, `pipetest`, `disktest`, `iotest`

### Built-in x86 Assembler (nasm)
- Full instruction set: `mov`, `add`, `sub`, `cmp`, `xor`, `and`, `or`
- Control flow: `jmp`, `je/jz`, `jne/jnz`, `call`, `ret`
- Stack: `push` (reg + imm + label), `pop`, `pusha`, `popa`
- Data: `db` with string + mixed format + numeric support
- **`times` directive** -- repeat instruction N times
- Segment registers: `mov ds, ax`, `mov ax, ds`
- External symbols via tabel: `_printf`, `_scanf`, `_malloc`, `_free`,
  `_sys_open`, `_sys_read`, `_sys_write_fd`, `_sys_close`, `_usr_printf`, dll
- Forward/backward label references + auto-patch
- Multi-page output buffer (up to 16KB)
- Tab support in editor (4 spaces)

### occ C Compiler
- Subset of C: `int`, `char`, `if/else`, `while`, `for`, function calls with params
- String literals, integer arithmetic, comparison operators
- Auto-generated assembly -> assembled by built-in assembler
- `printf`, `malloc`, `free`, `calloc`, `realloc` via external symbols
- Support `_sys_open`, `_sys_read`, `_sys_write_fd`, `_sys_close` for file I/O
- Array subscript `arr[i]` support
- Function parameters (`int add(int a, int b)`)
- Compiles with `occ /path/to/file.c`, runs via `nasm`

### Logging Infrastructure
- Circular buffer (4096 bytes)
- Auto-log exceptions with register dump (int_num, err_code, cr2, eip)
- `dmesg` command to view log
- Safe for interrupt context
- Works alongside VGA output

### User Space Library (usrlib)
- Functions using `int 0x80` syscalls internally
- `usr_printf`, `usr_puts`, `usr_putchar` -- output via syscall
- `usr_gets`, `usr_getchar` -- input via syscall
- `usr_malloc`, `usr_free` -- memory via `SYSCALL_BRK`
- Available as external symbols from built-in assembler

### Text Editor
- Nano-like editor: `edit /path/to/file`
- Arrow keys, Ctrl+S (save), Ctrl+X (exit), scrolling
- Status bar with filename, line, column
- Auto-save on exit

---

## Struktur Proyek

```
OaSis/
|-- src/
|   |-- boot/          # Entry point, linker script
|   |   |-- entry.asm
|   |   |-- linker.ld
|   |-- kernel/
|       |-- core/      # Kernel inti
|       |   |-- kernel.c       # Shell + main loop
|       |   |-- gdt.c          # GDT + TSS + user segments
|       |   |-- memory.c       # E820 memory detection
|       |   |-- paging.c       # Paging + process isolation
|       |   |-- pmm.c          # Physical memory manager
|       |   |-- vga.c          # VGA text mode driver
|       |-- drivers/   # Hardware drivers
|       |   |-- ata.c, block.c, idt.c, io.c
|       |   |-- keyboard.c, pic.c, timer.c
|       |-- fs/        # Filesystem
|       |   |-- fd.c          # File descriptor layer
|       |   |-- vfs.c         # OAFS filesystem (hardened + indirect)
|       |-- lib/       # Library
|       |   |-- string.c, lexer.c, parser.c, codegen.c
|       |   |-- klibc.c       # Kernel libc (printf/scanf)
|       |   |-- heap.c        # Kernel heap (kmalloc/kfree)
|       |   |-- log.c         # Logging infrastructure
|       |   |-- usrlib.c      # User space libc (via syscalls)
|       |-- syscall/   # System call layer
|       |   |-- syscall.c     # Dispatcher (23 syscalls)
|       |   |-- interrupt.asm # int 0x80 handler (ring 0+3)
|       |-- tasks/     # Task management
|       |   |-- task.c        # Scheduler + TCB + CR3 switching
|       |   |-- task_user.c   # User mode task creation
|       |-- apps/      # User applications (kernel-level)
|           |-- editor.c      # Text editor
|           |-- asm.c         # Built-in assembler
|-- include/           # Header files
|-- iso/               # GRUB boot files
|-- docs/              # Documentation (Jekyll)
|-- Makefile
|-- readme.md
```

---

## Cara Build & Run

### Prasyarat
```bash
sudo apt install gcc nasm grub2-common xorriso qemu-system-x86
```

### Build & Run
```bash
make          # build kernel.bin + oasis.iso
make clean    # clean build artifacts
make run      # boot di QEMU
```

### Test User Mode
```bash
# Di shell OaSis:
> edit /hello.asm
> user /hello.asm       # jalanin di ring 3
```

---

## Catatan Teknis

### User Mode Entry/Exit Flow
1. `user /file.asm` -> `asm_assemble()` -> `task_create_user()` -> `paging_create_user_dir()`
2. `switch_to_user()` -> `iret` ke ring 3 (CPU pop CS=0x1B, SS=0x23)
3. User code run with **dedicated page directory** (kernel pages hidden)
4. On `SYSCALL_USER_EXIT`: handler overwrites iret frame with kernel values
5. `iret` redirects to `user_return_to_shell` -> restore CR3 -> return to shell

### Memory Layout
```
0x00000000 - 0x003FFFFF   Identity map (kernel binary + BSS)
0x00400000 - 0x007FFFFF   Kernel extended map
0x00800000 - 0x00EFFFFF   User code / heap (process isolation)
0x00F00000 - 0x00FFFFFF   User stack (16KB)
0x01000000 - 0x02000000   User brk heap (~16MB)
0x02000000 - 0x03000000   Kernel heap (kmalloc)
0x40000000                CODE_VIRT (assembler code buffer)
0xC0000000                Higher-half kernel mapping
```

---

## Rencana Ke Depan

- [ ] **User space utilities via `occ`** -- port cat, ls, echo ke C
- [ ] **Better preemptive scheduler** -- real context save/restore
- [ ] **Framebuffer** -- VBE linear framebuffer graphics
- [ ] **Double fault handler** -- recover instead of triple fault
- [ ] **User space utilities via occ** -- cat, ls, echo in C
- [ ] **TCP/IP stack** -- networking support
- [ ] **Better preemptive scheduler** -- real context save/restore

---

## Lisensi

Bebas dipakai untuk belajar. No warranty -- ini OS edukasi, bukan production-ready.

---

**Dibikin dengan kopi, rasa penasaran, dan banyak debugging.** 
