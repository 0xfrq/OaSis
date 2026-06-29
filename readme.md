# OaSis

<p align="center">
  <b>x86 32-bit operating system from scratch.</b><br>
  <code>Ring 3 user mode</code> * <code>23 syscalls</code> * <code>Process isolation</code> * <code>OAFS filesystem</code> * <code>occ C compiler</code>
</p>

---

## Status

| Area | |
|------|--|
| Ring 3 User Mode | User code runs at ring 3, `user /file.asm` |
| Process Isolation | Per-task page directory + CR3 switching, user blocked from kernel pages |
| System Calls | 23 syscalls (0-22) via `int 0x80` + external symbol wrappers |
| Indirect Blocks | Files > 6KB (12 direct + 128 indirect = 70KB per file) |
| Kernel Heap | `kmalloc`/`kfree`/`kcalloc`/`krealloc` free-list allocator |
| Logging | Circular buffer, `dmesg` command, auto-log exception |
| Filesystem (OAFS) | Multi-block directory (168 entries), indirect blocks, color ls |
| occ Compiler | Subset C: `int`, `if/else`, `while`, `for`, function params, array subscript, `printf`, `malloc` |
| Built-in Assembler | NASM-style syntax, `times` directive, label push, segment registers |
| User Space Libc | `usr_printf`, `usr_malloc`, `usr_gets`, `usr_puts` via syscalls |
| Mini Libc | `printf`, `scanf`, `putchar`, `gets`, `sprintf`, `atoi` |
| GDT + TSS | User segments (0x18/0x20) + Task State Segment |
| Text Editor | Nano-like editor (`edit` command) |
| Drivers | PS/2 keyboard, VGA text mode, ATA/IDE PIO, PIT timer, block cache |

---

## Features

### Kernel & Boot
- Multiboot-compliant (GRUB-compatible)
- 32-bit protected mode (x86 i386)
- GDT with ring 0/ring 3 segments + TSS
- IDT for 32 interrupts + 16 IRQs + int 0x80 syscall handler
- PIC (Programmable Interrupt Controller)
- VGA text mode (80x25, 16 colors)

### Memory Management
- E820 memory detection
- Physical Memory Manager (PMM) — bitmap-based page allocator
- Paging (4KB pages) — identity map + higher-half kernel (0xC0000000+)
- Process isolation — each user task gets a clone of kernel page dir
- Per-task CR3 switching — task_switch() updates CR3 automatically
- Kernel heap allocator — free-list with splitting and coalescing
- `kmalloc`, `kfree`, `kcalloc`, `krealloc`

### User Mode (Ring 3)
- Ring 3 execution via `user /file.asm` command
- Dedicated page directory per user task
- 23 system calls via `int 0x80`
- User heap via `SYSCALL_BRK` for dynamic memory allocation
- Per-task fd_table

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
21  SYSCALL_USER_EXIT   - exit user mode, return to shell
22  SYSCALL_BRK         - expand/shrink user heap
```

### Filesystem (OAFS)
- Inode-based filesystem (Oasis File System)
- 1024 inodes, 8192 blocks, 512 bytes/block
- 12 direct block pointers + 1 indirect block (128 pointers) = ~70KB per file
- Directory support with path resolution (absolute + relative)
- Free block bitmap, inode table, superblock

### Shell
- Command-line interface with `oasis(/)` prompt
- Commands: `help`, `clear`, `uptime`, `meminfo`, `dmesg`, `syscall`
- File ops: `ls` (color-coded), `cd`, `pwd`, `mkdir`, `touch`, `rm`, `rmdir`, `cat`, `write`, `append`, `hexdump`
- Development: `edit`, `asm`, `nasm`, `occ`, `user`

### Built-in Assembler
- Instructions: `mov`, `add`, `sub`, `cmp`, `xor`, `and`, `or`
- Control flow: `jmp`, `je/jz`, `jne/jnz`, `call`, `ret`
- Stack: `push` (reg + imm + label), `pop`, `pusha`, `popa`
- Data: `db` with string + mixed format + numeric
- `times` directive
- Segment registers: `mov ds, ax`, `mov ax, ds`
- External symbol table: `_printf`, `_scanf`, `_malloc`, `_usr_printf`, etc.
- Forward/backward label references with auto-patch

### occ C Compiler
- Subset C: `int`, `char`, `if/else`, `while`, `for`, function calls with params
- String literals, integer arithmetic, comparison operators
- Auto-generated assembly, assembled by built-in assembler
- `printf`, `malloc`, `free` via external symbols
- Array subscript support
- Compile with `occ /path/to/file.c`, run via `nasm`

### User Space Library (usrlib)
- Functions using `int 0x80` syscalls internally
- `usr_printf`, `usr_puts`, `usr_putchar` — output via syscall
- `usr_gets`, `usr_getchar` — input via syscall
- `usr_malloc`, `usr_free` — memory via `SYSCALL_BRK`

### Text Editor
- Nano-like: `edit /path/to/file`
- Arrow keys, Ctrl+S (save), Ctrl+X (exit), scrolling
- Status bar with filename, line, column

---

## Project Structure

```
OaSis/
|-- src/
|   |-- boot/          # Entry point, linker script
|   |-- kernel/
|   |   |-- core/      # Kernel core
|   |   |-- drivers/   # Hardware drivers
|   |   |-- fs/        # Filesystem
|   |   |-- lib/       # Libraries (klibc, usrlib, occ, heap)
|   |   |-- syscall/   # System call layer
|   |   |-- tasks/     # Task management
|   |   |-- apps/      # User applications (editor, assembler)
|-- include/           # Header files
|-- iso/               # GRUB boot files
|-- docs/              # Documentation
|-- Makefile
|-- readme.md
```

---

## Build & Run

### Prerequisites
```bash
sudo apt install gcc nasm grub2-common xorriso qemu-system-x86
```

### Build & Run
```bash
make          # build kernel.bin + oasis.iso
make clean    # clean build artifacts
make run      # boot in QEMU
```

### Test User Mode
```
# In OaSis shell:
> edit /hello.asm
> user /hello.asm       # run at ring 3
```

---

## Technical Notes

### User Mode Entry/Exit Flow
1. `user /file.asm` → `asm_assemble()` → `task_create_user()` → `paging_create_user_dir()`
2. `switch_to_user()` → `iret` to ring 3 (CPU pops CS=0x1B, SS=0x23)
3. User code runs with dedicated page directory
4. On `SYSCALL_USER_EXIT`: handler overwrites iret frame with kernel values
5. `iret` redirects to `user_return_to_shell` → restore CR3 → return to shell

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

## Changelog

Recent fixes and additions are documented in [docs/10-changelog/index.md](docs/10-changelog/index.md).

---

## License

MIT — feel free to use and modify.

---

**Built with curiosity and a lot of debugging.**
