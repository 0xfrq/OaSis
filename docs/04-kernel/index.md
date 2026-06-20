---
layout: default
title: kernel
---

# 04. kernel

dokumentasi ini mencakup semua subsystem inti kernel oasis. kernel adalah bagian terbesar dari sistem dengan 33 file sumber.

## daftar isi

- [gdt -- global descriptor table](#gdt)
- [idt & interrupt handling](#idt)
- [memory management](#memory)
- [paging & process isolation](#paging)
- [task scheduler](#task-scheduler)
- [system calls](#system-calls)
- [heap allocator (kmalloc)](#heap-allocator)
- [filesystem -- oafs](#filesystem-oafs)
- [built-in assembler (asm.c)](#built-in-assembler)
- [klibc -- kernel libc](#klibc)
- [usrlib -- user space library](#usrlib)
- [logging infrastructure](#logging)
- [shell & command interface](#shell)

---

## gdt

gdt (global descriptor table) di-setup di `src/kernel/core/gdt.c` dan dipanggil dari `kernel_main()` paling awal sebelum idt. gdt di-inisialisasi dengan 6 entry yang overwrite gdt bawaan grub.

### layout gdt (6 entry)

```
0x00: NULL descriptor       -- required, gak dipake
0x08: kernel code (ring 0)  -- akses = 0x9A (present|ring0|code|execute/read)
0x10: kernel data (ring 0)  -- akses = 0x92 (present|ring0|data|read/write)
0x18: user code   (ring 3)  -- akses = 0xFA (present|ring3|code|execute/read)
0x20: user data   (ring 3)  -- akses = 0xF2 (present|ring3|data|read/write)
0x28: tss                  -- akses = 0x89, gran = 0x40
```

semua segmen flat: base=0, limit=4gb (0xFFFFF dengan granularity 4kb).

### tss (task state segment)

tss disimpan sebagai `static uint32_t tss[32]` di gdt.c. dua field yang penting:

```
tss[1] = kernel_esp  -- esp0, stack kernel buat ring3 -> ring0 transition
tss[2] = kernel_ss   -- ss0, = 0x10
```

tss selector (0x28) di-load ke task register via instruksi `ltrw`.

### fungsi gdt_set_entry()

```c
static void gdt_set_entry(int num, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low     = base & 0xFFFF;
    gdt_entries[num].base_mid     = (base >> 16) & 0xFF;
    gdt_entries[num].base_high    = (base >> 24) & 0xFF;
    gdt_entries[num].limit_low    = limit & 0xFFFF;
    gdt_entries[num].granularity  = (limit >> 16) & 0x0F;
    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access       = access;
}
```

### lgdt

```c
asm volatile(
    "lgdtl %0\n"
    "movl $0x10, %%eax\n"    /* reload ds/es/fs/gs dengan kernel data */
    "movw %%ax, %%ds\n"
    "movw %%ax, %%es\n"
    "movw %%ax, %%fs\n"
    "movw %%ax, %%gs\n"
    "ljmp $0x08, $.reload\n" /* far jump buat reload cs */
    ".reload:\n"
    : : "m"(gdt_ptr) : "eax", "memory"
);
```

selector constants (di `include/gdt.h`):
- `KERNEL_CS = 0x08`
- `KERNEL_DS = 0x10`
- `USER_CS   = 0x1B`  (0x18 | 0x03 = ring 3)
- `USER_DS   = 0x23`  (0x20 | 0x03 = ring 3)
- `TSS_SEL   = 0x28`

---

## idt

idt di-setup di `src/kernel/drivers/idt.c`. 256 entries, 8 byte per entry = 2048 byte total.

### layout

```
entry 0-31:   isr (cpu exceptions)
entry 32-47:  irq (hardware interrupts)
entry 48-127: unused
entry 128:    int 0x80 (syscall gate) -- dpl=3 (0xEF) biar bisa dari ring 3
```

### idt_set_entry()

```c
void idt_set_entry(int num, uint32_t handler, uint16_t selector, uint8_t type_attr) {
    idt[num].offset_lo = handler & 0xFFFF;
    idt[num].offset_hi = (handler >> 16) & 0xFFFF;
    idt[num].selector = selector;    /* 0x08 = kernel code */
    idt[num].type_attr = type_attr;  /* 0x8E = interrupt gate, 0xEF = gate dpl=3 */
    idt[num].reserved = 0;
}
```

### isr_common_stub (interrupt.asm)

semua isr 0-31 pake macro `ISR_NOERRCODE` atau `ISR_ERRCODE`, ujungnya lompat ke `isr_common_stub`.

stack layout setelah pusha + push ds:

```
[esp+0..28] = pusha: edi, esi, ebp, old_esp, ebx, edx, ecx, eax
[esp+32]   = ds (yang di-push)
[esp+36]   = error_code (atau 0 buat no-error-code)
[esp+40]   = int_number
[esp+44]   = eip (dari cpu push)
[esp+48]   = cs
[esp+52]   = eflags
[esp+56]   = user_esp (ring 3 only)
[esp+60]   = user_ss  (ring 3 only)
```

handler flow:
1. `pusha` -> simpen semua general registers
2. `push ds` -> reload ds dengan kernel data segment (0x10)
3. push error_code + int_number -> panggil `interrupt_handler()`
4. setelah return, pop ds, popa, add esp 8 (buang error code + int number)
5. `iret`

### interrupt_handler() di c

```c
void interrupt_handler(int int_num, int err_code) {
    /* baca cr2 kalo page fault */
    if (int_num == 14) {
        asm volatile("mov %%cr2, %0" : "=r"(cr2_val));
    }
    log_exception(int_num, err_code, cr2_val, eip);
    /* tampilkan ke vga */
    vga_print("=== EXCEPTION ===\n");
    vga_print("int_num="); ...
    vga_print(" err_code=0x"); ...
    if (int_num == 14) { vga_print("cr2=0x"); ... }
    vga_print("=== UNRECOVERABLE EXCEPTION ===\n");
    while (1) { asm volatile("cli; hlt"); }
}
```

### exception categories

| int | nama | error code | penyebab umum |
|-----|------|-----------|---------------|
| 0 | divide error | tidak | div/0 |
| 6 | invalid opcode | tidak | cpu jump ke data, instruksi gak dikenal |
| 13 | general protection fault | ya | akses segment invalid, ring violation, null selector |
| 14 | page fault | ya | akses page yang gak di-map atau gak punya hak |

### irq handlers

irq0 (timer): `timer_interrupt_handler()` -> increment ticks, panggil `task_switch()`.
irq1 (keyboard): `keyboard_interrupt_handler()` -> baca scancode dari port 0x60, konversi ke ascii, masukin ke circular buffer.

EOI (end of interrupt) dikirim via `outb(0x20, 0x20)` ke master pic sebelum iret.

### syscall handler (int 0x80)

detail di [system calls](#system-calls).

---

## memory

### physical memory manager (pmm)

di `src/kernel/core/pmm.c`. pake bitmap sederhana.

```
bitmap: 1MB byte = 8M bit, tiap bit = 1 page (4kb) -> bisa cover 32gb
```

inisialisasi (`pmm_init`):
1. set semua byte bitmap ke 0xFF (semua dipake)
2. loop dari page 0 sampai `total_pages = total_memory / 4096`: clear bit
3. mark page 0-0x100000 (first 1mb) sebagai used -- ini area kernel
4. mark page 0x100000 - `_end` (kernel binary) sebagai used

`_end` diambil dari linker script via `extern uint32_t _end;`. kernel binary dari 0x100000 sampe `_end` (sekitar 2.7mb).

fungsi:
- `pmm_alloc_page()` -- linear scan bitmap buat cari free page, return physical address
- `pmm_free_page(phys)` -- clear bit, increment free_pages counter
- `pmm_get_free_pages()` -- return free_pages

### paging

di `src/kernel/core/paging.c`. 4kb pages, 2-level hierarchy.

```
page directory (1024 pde)   -- tiap pde 4 byte, total 4kb
  +-> page table (1024 pte) -- tiap pte 4 byte, total 4kb
       +-> physical page (4096 byte)
```

kernel page dir: `pde_t kernel_page_dir[1024] __attribute__((aligned(0x1000)))`.

in `paging_init`:
1. identity map pde[0] (0x00000000-0x003FFFFF) -- kernel code ada di 1MB-3MB, ini penting karena stack, gdt, idt, dll ada di range ini
2. map pde[0xC00..0xC03] (0xC0000000-0xC0400000) -- higher-half kernel mapping, fisiknya sama dengan identity map

kedua mapping ini pake physical pages dari `kernel_page_tables[0..4]`.

`page_map(virt, phys, flags)`:
1. `dir_index = virt >> 22`, `table_index = (virt >> 12) & 0x3FF`
2. kalo pde gak present, alloc page table baru dari pool `kernel_page_tables`
3. set `pt[table_index] = (phys & PAGE_MASK) | flags | PTE_PRESENT`
4. kalo flags mengandung PTE_USER, set juga di pde

`page_table_index` makin bertambah tiap kali alloc page table baru. pool awal 10, skrg 128.

### process isolation (paging_create_user_dir)

di `paging.c`, bikin clone kernel page directory buat user task:

1. alloc physical page buat page directory baru
2. map sementara di 0x00300000
3. clone setiap pde dari kernel_page_dir:
   - kalo pde index 0 (identity map) atau 0xC00-0xC03 (higher-half): copy pte **tanpa** PTE_USER
   - kalo pde index lainnya: copy pte **dengan** PTE_USER
4. return physical address buat di-load ke cr3

waktu switch ke user page dir, kernel masih bisa akses semua memory (ring 0), tapi user cuma bisa akses page yang ada PTE_USER.

### kernel heap (kmalloc)

di `src/kernel/lib/heap.c`. free-list allocator dengan splitting & coalescing.

block header (`heap_block_t`, 16 byte):
```c
uint32_t size;           /* total ukuran block (header + payload) */
uint16_t magic;          /* MAGIC_FREE = 0xF4EE, MAGIC_USED = 0x1CED */
uint16_t flags;          /* bit 0: 1 = free, 0 = used */
heap_block_t *next;      /* next/prev di free list */
heap_block_t *prev;
```

heap region: `0x02000000 - 0x03000000` (16mb max). initial heap 64kb.

`kmalloc(size)`:
1. kalo blm init, panggil `heap_init()` -> alloc 64kb (16 pages) via pmm
2. `needed = align_up(sizeof(header) + size)`
3. first-fit scan free list
4. kalo block cukup gede (>= needed + BLOCK_MIN), split jadi dua block
5. kalo gak ketemu, expand heap: alloc page baru, coalesce sama block terakhir kalo adjacent

`kfree(ptr)`:
1. validasi magic number
2. set jadi free, tambah ke free list (sorted by address)
3. coalesce dengan adjacent free blocks

splitting: block -> [used block] [free block]
coalescing: [free A] + [free B (adjacent)] -> [free A+B]

---

## task scheduler

di `src/kernel/tasks/task.c`. struktur task:

```c
typedef struct task_t {
    uint32_t id;
    uint32_t ppid;
    task_state_t state;       /* READY, RUNNING, BLOCKED, DEAD */
    int exit_code;
    task_context_t context;   /* eax, ebx, ecx, edx, esi, edi, ebp, esp, eip, eflags, cs, cr3 */
    uint32_t *stack;
    uint32_t stack_base;
    struct task_t *next;
    struct task_t *prev;
    fd_table_t *fd_table;
} task_t;
```

task disimpan di array global `task_t tasks[TASK_MAX]` dengan `TASK_MAX = 16`. linked circular via next/prev pointer.

### task_create()

1. alloc stack (4 page = 16kb) + mapping `PTE_PRESENT | PTE_WRITE | PTE_USER`
2. init context: ebp = stack top - 4, eip = entry function, eflags = 0x202, cs = 0x08
3. init fd_table dari `task_fd_tables[]` statis
4. link ke circular list task

### scheduler

timer irq (100hz) -> `timer_interrupt_handler()` -> `task_switch()`.

`task_switch()`:
1. kalo task_count <= 1, return
2. `current_task = current_task->next` (round-robin)
3. set state = TASK_RUNNING
4. kalo current_task punya cr3, panggil `paging_switch_dir()`

**tidak ada** context save/restore yang sesungguhnya -- ini kelemahan. scheduler cuma ganti current_task pointer dan cr3, tapi gak nyimpen/restore register task sebelumnya. iret di irq handler balik ke task yang sama meskipun current_task udah ganti.

### task_fork()

copy task (termasuk stack), copy fd_table parent ke child.

### task_user.c

bikin user mode task:
1. ambil code dari CODE_VIRT (0x40000000)
2. map page dengan PTE_USER (biar bisa diakses ring 3)
3. alloc stack di 0xF00000 dengan PTE_USER
4. setup tcb dengan cs = USER_CS (0x1B)
5. `switch_to_user()` via iret:

```c
asm volatile(
    "pushl %2\n"      /* ss = user_data segment (0x23) */
    "pushl %1\n"      /* esp = user stack */
    "pushl %3\n"      /* eflags = 0x202 */
    "pushl %0\n"      /* cs = user_code segment (0x1B) */
    "pushl %4\n"      /* eip = entry point */
    "mov %2, %%eax\n"
    "mov %%eax, %%ds\n"
    "mov %%eax, %%es\n"
    "mov %%eax, %%fs\n"
    "mov %%eax, %%gs\n"
    "iret\n"
);
```

setelah iret, cpu berjalan di ring 3 dengan user code.

### user return to shell

`user_return_to_shell` (di interrupt.asm):
1. restore cr3 ke `kernel_page_dir`
2. restore esp dari `user_exit_esp`
3. pop ebp
4. ret -> balik ke kernel_main shell loop

---

## system calls

23 system calls via int 0x80. handler di `interrupt.asm`, dispatcher di `syscall.c`.

### int_80_wrapper -- deteksi ring

```asm
int_80_wrapper:
    cli
    pusha
    cmp dword [esp + 36], 0x08  /* cs di [esp+36] */
    je .ring0                   /* 0x08 = kernel, 0x1B = user */
```

ring 0 path:
- cpu push: eip, cs, eflags (3 words)
- handler: push args -> call -> store return -> popa -> iret

ring 3 path:
- cpu push: ss, esp, eflags, cs, eip (5 words)
- handler: sama, tapi setelah itu cek `user_exit_flag`
- kalo exit, overwrite iret frame dengan address kernel untuk redirect ke shell

### syscall table (23 syscall)

| # | nama | arg | deskripsi |
|---|------|-----|-----------|
| 0 | SYSCALL_WRITE | msg, len | tulis ke stdout (legacy) |
| 1 | SYSCALL_SLEEP | ms | sleep (stub doang) |
| 2 | SYSCALL_YIELD | - | yield scheduler |
| 3 | SYSCALL_EXIT | exit_code | exit task, close semua fd |
| 4 | SYSCALL_GETPID | - | return current_task->id |
| 5 | SYSCALL_FORK | - | fork process |
| 6 | SYSCALL_EXEC | program, size | exec (stub) |
| 7 | SYSCALL_WAIT | status | wait child |
| 8 | SYSCALL_GETPPID | - | return ppid |
| 9 | SYSCALL_OPEN | path, flags | open file via fd layer |
| 10 | SYSCALL_CLOSE | fd | close fd |
| 11 | SYSCALL_READ | fd, buf, count | read dari fd |
| 12 | SYSCALL_WRITE_FD | fd, buf, count | write ke fd |
| 13 | SYSCALL_PIPE | pipefd[2] | pipe |
| 14 | SYSCALL_DUP | oldfd | duplicate fd |
| 15 | SYSCALL_DUP2 | oldfd, newfd | dup2 |
| 16 | SYSCALL_SEEK | fd, offset, whence | seek |
| 17 | SYSCALL_FDINFO | - | debug: print fd table |
| 18 | SYSCALL_BLOCK_READ | block, buf | read block device |
| 19 | SYSCALL_BLOCK_WRITE | block, buf | write block device |
| 20 | SYSCALL_BLOCK_FLUSH | - | flush block cache |
| 21 | SYSCALL_USER_EXIT | - | exit user mode, redirect ke shell |
| 22 | SYSCALL_BRK | addr | user heap expansion |

### external symbols (asm.c)

tabel `extern_syms[]` di `asm.c`:

| symbol | fungsi | asal |
|--------|--------|------|
| `_printf` | `klibc_printf` | klibc.c |
| `_putchar` | `klibc_putchar` | klibc.c |
| `_puts` | `klibc_puts` | klibc.c |
| `_malloc` | `kmalloc` | heap.c |
| `_free` | `kfree` | heap.c |
| `_sys_open` | `syscall_open` | syscall.c |
| `_sys_read` | `syscall_read` | syscall.c |
| `_sys_write_fd` | `syscall_write_fd` | syscall.c |
| `_usr_printf` | `usr_printf` | usrlib.c |

resolusi external symbol:
```c
uint32_t ext_addr = find_extern(name);
/* hitung relative call */
int32_t rel = (int32_t)(ext_addr - CODE_VIRT - patches[i].from);
```

kalo label gak ditemukan di local `labels[]`, assembler cari di `extern_syms[]`.

### user exit flow

1. user panggil `syscall 21` (SYSCALL_USER_EXIT)
2. C handler set `user_exit_flag = 1`
3. assembly handler deteksi flag, overwrite iret frame:
   - eip = `user_exit_eip` (address user_return_to_shell)
   - cs = 0x08 (kernel code)
   - esp = `user_exit_esp` (kernel stack)
   - ss = 0x10 (kernel data)
4. iret -> balik ke kernel mode di `user_return_to_shell`
5. restore cr3, pop ebp, ret -> balik ke shell

---

## heap allocator (kmalloc)

free-list allocator di `src/kernel/lib/heap.c`.

algoritma sudah dijelaskan di [memory section](#memory). tambahan:

### heap expansion

kalo free list kosong (gak ada block yang cukup):
1. `expand_heap(size)` -> alloc N page via pmm, map di heap boundary
2. buat block baru dari page yang baru
3. coalesce dengan free block terakhir kalo adjacent

### `kcalloc(n, size)`

`kmalloc(n * size)` + `memset(ptr, 0, total)`.

### `krealloc(ptr, new_size)`

1. kalo ptr null, return `kmalloc(new_size)`
2. kalo new_size 0, `kfree(ptr)`, return null
3. alloc baru, `memcpy` min(old_size, new_size), kfree(ptr)

---

## filesystem (oafs)

di `src/kernel/fs/vfs.c`. inode-based filesystem kustom.

### on-disk layout

```
[block 128: superblock] [block 129..: inode table] [block ..8192: data blocks]
```

superblock:
```c
uint32_t magic;         /* VFS_MAGIC = 0x0AF6 */
uint32_t total_blocks;  /* 8192 */
uint32_t total_inodes;  /* 1024 */
uint32_t free_inodes;
uint32_t free_blocks;
```

inode:
```c
uint32_t type;          /* 0=free, 1=file, 2=dir */
uint32_t size;
uint32_t parent_inode;
uint32_t direct[12];    /* direct block pointers */
uint32_t indirect;      /* indirect block pointer */
uint32_t ctime, mtime;
char name[32];
```

kapasitas file: 12 direct * 512 = 6kb + 128 indirect * 512 = 64kb, total ~70kb.

directory entry:
```c
uint32_t inode_number;
char name[MAX_FILENAME_LENGTH];  /* 32 */
```

1 block = 512 byte, tiap entry 36 byte -> max 14 entry per block. tapi sekarang multi-block directory support sampe 12 block = 168 entries.

### key functions

`vfs_open(path, flags)`: resolve path, kalo gak ada + O_CREATE -> create file.
`vfs_read(fd, buf, count)`: pake `get_block_ptr()` buat dapetin block number dari offset.
`vfs_write(fd, buf, count)`: pake `set_block_ptr()` buat alokasi block otomatis.
`vfs_unlink(path)`: remove directory entry -> free blocks -> free inode.
`vfs_mkdir(path)`: alloc inode + block buat directory entries.
`vfs_resolve_path(path, &ino)`: parse path, cari child di setiap directory.

### fd layer

di `src/kernel/fs/fd.c`. nyediain file descriptor abstraction di atas vfs.

`fd_open()`: konversi posix flags ke vfs flags, panggil `vfs_open()`, setup fd entry.
`fd_read()`: ambil vfs_fd dari entry, panggil `vfs_read()`.
`fd_write()`: sama.

open file limit: `FD_MAX = 32`.

---

## built-in assembler (asm.c)

assembler x86 32-bit lengkap di `src/kernel/apps/asm.c`, ~1400 baris.

### komponen

1. **code buffer**: `static uint8_t code_buf[CODE_SIZE]` dengan `CODE_SIZE = 16384`
2. **label table**: 32 label max
3. **patch table**: 128 patch max untuk forward/backward reference
4. **external symbol table**: fungsi kernel & usrlib yang bisa dipanggil

### instruksi yang didukung

| kategori | instruksi |
|----------|-----------|
| move | `mov` (reg, mem, imm, seg reg) |
| arithmetic | `add`, `sub`, `cmp`, `xor`, `and`, `or` |
| control | `jmp`, `je/jz`, `jne/jnz`, `jg/jl/jge/jle`, `call`, `ret` |
| stack | `push` (reg, imm, label), `pop`, `pusha`, `popa` |
| other | `int`, `nop`, `hlt`, `sti`, `cli`, `inc`, `dec`, `imul`, `idiv`, `cdq`, `test`, `movzx`, `neg`, `div`, `setcc`, `cmovcc` |
| data | `db` (mixed string + numeric) |
| directive | `times N <instruction>` |

### gen_push label support

kalo operand bukan register dan bukan integer (gagal `parse_int`), assembler coba resolve sebagai label:

```c
int target = find_label(ops);
if (target >= 0) {
    emit(0x68); emit32(CODE_VIRT + target);
    return 0;
}
add_patch(code_len + 1, 0, ops, 2);  /* forward reference */
emit(0x68); emit32(0);
```

### asm_assemble flow

1. copy code ke `input_buf[]`
2. proses baris per baris via `process_line()`
3. set `code_len` = total byte yang di-emit
4. apply patches (resolve forward references)
5. `pmm_alloc_page()` -> `page_map(CODE_VIRT, phys, PTE_PRESENT|PTE_WRITE|PTE_USER)`
6. copy `code_buf` ke `CODE_VIRT`

code dijalankan dengan `void (*fn)(void) = (void(*)(void))CODE_VIRT; fn();`.

---

## klibc

kernel-space library di `src/kernel/lib/klibc.c`. fungsi standard c yang jalan di kernel mode (ring 0), pake `vga_putc`/`vga_print` langsung.

### fungsi output

```c
int klibc_putchar(int c);   /* vga_putc(c) */
int klibc_puts(const char *s);  /* vga_print(s) + newline */
int klibc_printf(const char *fmt, ...);  /* printf dengan %d %s %c %x %u %o %% */
int klibc_sprintf(char *buf, const char *fmt, ...);  /* sprintf ke buffer */
```

printf support: `%d %i %u %x %X %s %c %o %p %%`, width, left-align, zero-pad.

### fungsi input

```c
int klibc_getchar(void);     /* keyboard_getchar() + echo */
char *klibc_gets(char *s);   /* baca sampe \n, handle backspace */
int klibc_scanf(const char *fmt, ...);  /* scanf dengan %d %s %c %x %o */
```

### utility

```c
int klibc_atoi(const char *s);  /* string to int */
```

klibc_printf pake `va_list` dan `va_arg` untuk handling variadic arguments.

---

## usrlib

user space library di `src/kernel/lib/usrlib.c`. fungsi yang sama kaya klibc tapi pake `int 0x80` syscall, jadi aman dipanggil dari ring 3.

### fungsi

```c
int usr_printf(const char *fmt, ...);  /* printf via syscall */
int usr_puts(const char *s);           /* puts via syscall */
int usr_putchar(int c);                /* putchar via syscall */
int usr_getchar(void);                 /* getchar via syscall */
char *usr_gets(char *s);              /* gets via syscall */
void *usr_malloc(uint32_t size);      /* malloc via brk syscall */
void usr_free(void *ptr);             /* stub */
```

setiap fungsi pake inline syscall wrapper:
```c
static inline uint32_t sys3(uint32_t num, uint32_t a1, uint32_t a2, uint32_t a3) {
    register uint32_t eax asm("eax") = num;
    register uint32_t ebx asm("ebx") = a1;
    register uint32_t ecx asm("ecx") = a2;
    register uint32_t edx asm("edx") = a3;
    asm volatile("int $0x80" : "+r"(eax) : "r"(ebx), "r"(ecx), "r"(edx));
    return eax;
}
```

---

## logging

di `src/kernel/lib/log.c`. circular buffer 4096 byte.

```c
static char log_buf[LOG_BUF_SIZE];  /* LOG_BUF_SIZE = 4096 */
static volatile int write_pos = 0;
static volatile int read_pos = 0;
```

### log_printf()

format: `[timestamp] message\n`. timestamp pake tick counter dari timer.

support format: `%s`, `%d`, `%x`, `%%`. kalo buffer penuh, write_pos maju, read_pos juga ikut maju (overflow).

### log_exception()

dipanggil dari `interrupt_handler()` di idt.c. format:
```
[EXC] int=N err=0xX cr2=0xX eip=0xX
```

### dmesg

shell command `dmesg` -> `log_dump()` -> print semua isi circular buffer ke vga.

---

## shell

shell berjalan sebagai infinite loop di `kernel_main()` (`src/kernel/core/kernel.c`).

```c
while (1) {
    char c = keyboard_getchar();
    if (c == '\n') {
        input[index] = 0;
        /* parse command */
        if (strcmp(input, "help") == 0) { ... }
        else if (starts_with(input, "edit ")) { ... }
        else if (starts_with(input, "cat ")) { ... }
        /* ... */
    } else if (c == '\b') {
        if (index > 0) index--;
    } else {
        if (index < INPUT_MAX - 1) input[index++] = c;
    }
}
```

### command list

| command | implementasi |
|---------|-------------|
| `help` | print daftar command |
| `clear` | `vga_clear()` |
| `ls [path]` | `vfs_list()` -> color coded output |
| `cd <path>` | `vfs_chdir()` |
| `pwd` | `vfs_getcwd()` |
| `mkdir <p>` | `vfs_mkdir()` |
| `touch <p>` | `vfs_create()` |
| `rm <p>` | `vfs_unlink()` |
| `rmdir <p>` | `vfs_rmdir()` |
| `cat <p>` | `vfs_open(O_RDONLY)` -> `vfs_read()` -> print |
| `write <p> <t>` | `vfs_open(O_WRITE|O_CREATE|O_TRUNC)` -> `vfs_write()` |
| `append <p> <t>` | same with O_APPEND |
| `echo <text>` | print teks |
| `hexdump <p>` | read file, print hex |
| `edit <p>` | editor_run() |
| `nasm <p>` | asm_run_file() |
| `user <p>` | run_user_test() |
| `occ <p>` | run_occ() |
| `dmesg` | log_dump() |
| `syscall` | print syscall table |
| `uptime` | timer_get_ticks() / 100 |
| `meminfo` | pmm_get_free_pages() |
| `taskinfo` | task_print_info() |

### ls color

parser output dari `vfs_list()`:

```c
if (out[j] == 'd') {
    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);  /* directory = kuning */
} else if (out[j] == 'f') {
    vga_set_color(15, VGA_COLOR_BLACK);                 /* file = putih */
}
```
