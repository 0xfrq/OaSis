---
layout: default
title: kernel
---

# Kernel

documentation this covers all subsystem kernel oasis. kernel is part terbesar with beberapa file sumber (c, assembly, header).

## GDT: global descriptor table

file: `src/kernel/core/gdt.c`, header: `include/gdt.h`

### Structure

entry gdt, each entry :

```c
typedef struct {
 uint16_t limit_low; // limit bawah
 uint16_t base_low; // base bawah
 uint8_t base_mid; // base tengah
 uint8_t access; // access byte (present, dpl, type)
 uint8_t granularity; // granularity + limit atas
 uint8_t base_high; // base atas
} __attribute__((packed)) gdt_entry_t;
```

### Access byte format

bit:
- 7: present (harus 1)
- 6-5: dpl (00=ring0, 11=ring3)
- 4: s (1=code/data, 0=system)
- 3: type (code=1, data=0)
- 2: c/d (code: conforming, data: direction)
- 1: r/w (code: readable, data: writable)
- 0: a (accessed, set oleh cpu)

code segment 0x9A = 10011010
- present | dpl 0 | code/data | code | execute/read

data segment 0x92 = 10010010
- present | dpl 0 | code/data | data | read/write

user code 0xFA = 11111010
- present | dpl 3 | code/data | code | execute/read

user data 0xF2 = 11110010
- present | dpl 3 | code/data | data | read/write

### TSS

```c
static uint32_t TSS[32] = {0}; // , enough for 26 uint32_t
```

field penting:
- TSS[1] = esp0 (kernel stack pointer)
- TSS[2] = ss0 (kernel data segment = 0x10)

TSS descriptor in the GDT:
```c
gdt_set_entry(5, (uint32_t)&TSS, sizeof(TSS) - 1, 0x89, 0x40);
// 0x89 = present | ring0 | TSS (0x9 = available TSS)
// 0x40 = byte granularity, 32-bit
```

### Gdt_init()

1. set gdt_ptr.limit and .base
2. set entry 0-5
3. memanggil `lgdtl`
4. reload segment registers (ds/es/fs/gs = 0x10)
5. far jump to 0x08 for reload cs
6. `ltrw` with selector TSS = 0x28

### Reload assembly

```asm
lgdtl [gdt_ptr]
movl $0x10, %eax
movw %ax, %ds
movw %ax, %es
movw %ax, %fs
movw %ax, %gs
ljmp $0x08, $.reload
.reload:
```

ltrw:
```asm
mov $0x28, %ax
ltr %ax
```

## IDT: interrupt descriptor table

file: `src/kernel/drivers/idt.c`, `src/kernel/syscall/interrupt.asm`

### Structure idt entry

```c
typedef struct {
 uint16_t offset_lo; // handler address bawah
 uint16_t selector; // code segment selector (0x08)
 uint8_t reserved; // always 0
 uint8_t type_attr; // type and attributes
 uint16_t offset_hi; // handler address atas
} __attribute__((packed)) IDTEntry;
```

### IDT entries

| range | count | type | type_attr | deskripsi |
|-------|-------|------|-----------|-----------|
| 0-31 | 32 | isr | 0x8E | cpu exceptions |
| 32-47 | 16 | irq | 0x8E | hardware interrupts |
| 128 | 1 | syscall | 0xEF | int 0x80 (dpl=3) |

0x8E = present | ring 0 | interrupt gate (1110)
0xEF = present | ring 3 | interrupt gate (1111)

### Isr macro

macros in interrupt.asm generate 32 isr handler:

```asm
%macro ISR_NOERRCODE 1
[GLOBAL isr_%1]
isr_%1:
 push byte 0 ; dummy error code
 push byte %1 ; interrupt number
 jmp isr_common_stub
%endmacro

%macro ISR_ERRCODE 1
[GLOBAL isr_%1]
isr_%1:
 push byte %1 ; interrupt number (error code from cpu)
 jmp isr_common_stub
%endmacro
```

that using errcode: 8 (double fault), 10 (invalid TSS), 11 (segment not present), 12 (stack segment), 13 (gpf), 14 (page fault).

### Isr_common_stub

```asm
isr_common_stub:
 pusha
 push ds
 mov ax, 0x10
 mov ds, es, fs, gs ; reload kernel data segments

 ; call C handler
 mov eax, [esp + 40] ; err_code
 push eax
 mov eax, [esp + 40] ; int_num
 push eax
 call interrupt_handler
 add esp, 8

 pop eax
 mov ds, es, fs, gs
 popa
 add esp, 8 ; discard err_code and int_num
 iret
```

### Stack layout in isr_common_stub (ring 0 case)

```text
[esp+0] = edi (pusha)
[esp+4] = esi
[esp+8] = ebp
[esp+12] = old_esp
[esp+16] = ebx
[esp+20] = edx
[esp+24] = ecx
[esp+28] = eax
[esp+32] = ds (yang di-push)
[esp+36] = error_code (0 kalo no-error)
[esp+40] = int_number
[esp+44] = eip (cpu push)
[esp+48] = cs
[esp+52] = eflags
```

if from ring 3, add user_esp at [esp+56] and user_ss at [esp+60].

### Interrupt_handler

di idt.c. function c that called from isr_common_stub.

```c
void interrupt_handler(int int_num, int err_code) {
 uint32_t cr2_val = 0;
 if (int_num == 14) { // page fault
 asm volatile("mov %%cr2, %0" : "=r"(cr2_val));
 }
 // ambil eip (kurang akurat, tapi buat debugging cukup)
 uint32_t eip_val = 0;
 asm volatile("mov 4(%%ebp), %0" : "=r"(eip_val));

 log_exception(int_num, err_code, cr2_val, eip_val);

 // display on screen
 vga_print("=== EXCEPTION ===\n");
 // print int_num, err_code, cr2 (kalo page fault)

 // eoi buat irq
 if (int_num >= 32 && int_num < 48) {
 outb(0x20, 0x20);
 if (int_num >= 40) outb(0xA0, 0x20);
 }

 vga_print("=== UNRECOVERABLE EXCEPTION ===\n");
 while (1) { asm volatile("cli; hlt"); }
}
```

### Irq handlers

**irq_0 (timer)**:
```asm
irq_0:
 cli
 pusha
 push ds
 mov ax, 0x10
 mov ds, es, fs, gs
 call timer_interrupt_handler ; ticks++ + task_switch()
 mov al, 0x20
 out 0x20, al ; eoi
 pop eax
 mov ds, es, fs, gs
 popa
 sti
 iret
```

**irq_1 (keyboard)**:
the same, but it calls `keyboard_interrupt_handler()` -> reads the scancode from port 0x60, converts it and stores it in circular buffer.

### Int 0x80 handler -- deteksi ring

```asm
int_80_wrapper:
 cli
 pusha
 cmp dword [esp + 36], 0x08 ; CS at [esp+36]
 je .ring0 ; 0x08 = ring 0, 0x1B = ring 3

; ---- ring 3 ----
 mov eax, [esp + 28] ; syscall_num
 mov ebx, [esp + 16] ; arg1
 mov ecx, [esp + 24] ; arg2
 mov edx, [esp + 20] ; arg3
 push edx, ecx, ebx, eax
 call int_80_handler
 add esp, 16
 mov [esp + 28], eax ; return value

 ; cek exit request
 cmp dword [user_exit_flag], 0
 je .r3_noexit

.r3_exit:
 ; redirect to kernel mode
 mov eax, [user_exit_eip]
 mov ebx, [user_exit_esp]
 mov [esp + 32], eax ; overwrite eip
 mov dword [esp + 36], 0x08 ; cs = kernel code
 mov dword [esp + 40], 0x202 ; eflags
 mov [esp + 44], ebx ; esp = kernel stack
 mov dword [esp + 48], 0x10 ; ss = kernel data
 mov dword [user_exit_flag], 0

.r3_noexit:
 popa
 sti
 iret

; ---- ring 0 ----
.ring0:
 ; load arguments from pusha
 mov eax, [esp + 28] ; syscall_num
 mov ebx, [esp + 16] ; arg1
 mov ecx, [esp + 24] ; arg2
 mov edx, [esp + 20] ; arg3
 push edx, ecx, ebx, eax
 call int_80_handler
 add esp, 16
 mov [esp + 28], eax
 popa
 sti
 iret
```

### User_return_to_shell

```asm
user_return_to_shell:
 mov eax, kernel_page_dir
 mov cr3, eax ; restore kernel page dir
 mov esp, [user_exit_esp]
 pop ebp
 ret ; return to the shell loop
```

## System calls

file: `src/kernel/syscall/syscall.c`

### Dispatcher

```c
uint32_t syscall_dispatch(uint32_t num, uint32_t a1, uint32_t a2, uint32_t a3) {
 switch (num) {
 case SYSCALL_WRITE: return syscall_write(...);
 case SYSCALL_OPEN: return syscall_open(...);
 case SYSCALL_READ: return syscall_read(...);
 // ... 23 cases total
 case SYSCALL_USER_EXIT: return syscall_user_exit();
 case SYSCALL_BRK: return syscall_brk(a1);
 default: return 0xFFFFFFFF; // invalid
 }
}
```

### Syscall_open

```c
uint32_t syscall_open(const char *path, int flags) {
 fd_table_t *table = fd_get_current_table();
 return fd_open(table, path, flags);
}
```

### Syscall_brk (user heap)

```c
# Define USER_HEAP_BASE 0x01000000
# Define USER_HEAP_MAX 0x02000000
static uint32_t user_brk = USER_HEAP_BASE;

static uint32_t syscall_brk(uint32_t addr) {
 if (addr == 0) return user_brk; // query current
 if (addr < USER_HEAP_BASE || addr > USER_HEAP_MAX) return user_brk;

 addr = (addr + 0xFFF) & ~0xFFF; // align to a page

 // expand upward
 while (user_brk < addr) {
 uint32_t phys = PMM_alloc_page();
 page_map(user_brk, phys, PTE_PRESENT | PTE_WRITE | PTE_USER);
 user_brk += 0x1000;
 }
 // shrink (physical pages are not freed yet)
 while (user_brk > addr) { user_brk -= 0x1000; }

 return user_brk;
}
```

### Syscall_user_exit

```c
static uint32_t syscall_user_exit(void) {
 user_exit_flag = 1;
 return 0;
}
```

handler assembly liat `user_exit_flag != 0`, redirect iret to `user_return_to_shell`.

### Daftar lengkap syscall

| # | nama | argumen | return | deskripsi |
|---|------|---------|--------|-----------|
| 0 | SYSCALL_WRITE | msg, len | bytes | write to stdout (legacy) |
| 1 | SYSCALL_SLEEP | ms | 0 | sleep (stub) |
| 2 | SYSCALL_YIELD | - | 0 | yield task |
| 3 | SYSCALL_EXIT | code | 0 | exit, close fd, mark dead |
| 4 | SYSCALL_GETPID | - | pid | current_task->id |
| 5 | SYSCALL_FORK | - | child_pid | fork process |
| 6 | SYSCALL_EXEC | prog, size | 0 | exec program (stub) |
| 7 | SYSCALL_WAIT | status | child_pid | wait child |
| 8 | SYSCALL_GETPPID | - | ppid | parent pid |
| 9 | SYSCALL_open | path, flags | fd | open file |
| 10 | SYSCALL_close | fd | 0 | close fd |
| 11 | SYSCALL_read | fd, buf, cnt | bytes | read from fd |
| 12 | SYSCALL_WRITE_FD | fd, buf, cnt | bytes | write to the fd |
| 13 | SYSCALL_PIPE | pipefd[2] | 0 | create pipe |
| 14 | SYSCALL_DUP | oldfd | newfd | duplicate fd |
| 15 | SYSCALL_DUP2 | oldfd, newfd | 0 | duplicate to a specific fd |
| 16 | SYSCALL_SEEK | fd, off, whence | 0 | seek on fd |
| 17 | SYSCALL_FDINFO | - | 0 | print fd table |
| 18 | SYSCALL_block_read | block, buf | 0 | read block device |
| 19 | SYSCALL_block_write | block, buf | 0 | write block device |
| 20 | SYSCALL_block_FLUSH | - | 0 | flush block cache |
| 21 | SYSCALL_USER_EXIT | - | 0 | exit user mode -> shell |
| 22 | SYSCALL_BRK | addr | brk | user heap expansion |

## Memory management

### PMM (physical memory manager)

file: `src/kernel/core/PMM.c`

bitmap 1mb (8mb bit) can cover 32gb memory.

```c
# Define BITMAP_SIZE 1024 * 1024 // 1mb
static uint8_t page_bitmap[BITMAP_SIZE];
static uint32_t total_pages = 0;
static uint32_t free_pages = 0;
```

#### PMM_init(total_memory)

1. set all bytes to 0xFF (all pages used)
2. `total_pages = total_memory / 4096`
3. loop from page 0 sampe total_pages: clear bit (mark free)
4. mark page 0-0x100000 (first 1mb) as used
5. mark page 0x100000 - _end (kernel binary) as used

#### PMM_alloc_page()

```c
for (i = 0; i < total_pages; i++) {
 if (!bitmap_test(i)) { // free bit
 bitmap_set(i);
 free_pages--;
 return i * PAGE_SIZE; // physical address
 }
}
return 0; // no free pages
```

bitmap_test: `!(page_bitmap[byte] & (1 << bit))`

#### PMM_free_page(phys)

```c
uint32_t page = phys / PAGE_SIZE;
bitmap_clear(page);
free_pages++;
```

### Paging

file: `src/kernel/core/paging.c`

```c
__attribute__((aligned(0x1000)))
pde_t kernel_page_dir[PAGE_DIR_SIZE]; // entry
static pte_t kernel_page_tables[128][PAGE_TABLE_SIZE]; // page table pool
static int page_table_index = 5; // 0-4 udah dipake init
```

#### Paging_init

1. clear all pde
2. pde[0]: identity map 0-4mb
 - using `kernel_page_tables[0]`
 - each pte: `(i * 4096) | PTE_PRESENT | PTE_write`
3. pde[0xC00..0xC03]: higher-half
 - using `kernel_page_tables[1..4]`
 - mapping fisik that same with identity map

#### Page_map(virt, phys, flags)

```c
uint32_t dir_index = virt >> 22;
uint32_t table_index = (virt >> 12) & 0x3FF;

if (!kernel_page_dir[dir_index] & PTE_PRESENT) {
 // allocate a new page table from the pool
 pte_t *pt = kernel_page_tables[page_table_index++];
 for (int i = 0; i < 1024; i++) pt[i] = 0;
 kernel_page_dir[dir_index] = (uint32_t)pt | PTE_PRESENT | PTE_WRITE;
 if (flags & PTE_USER) kernel_page_dir[dir_index] |= PTE_USER;
} else {
 if (flags & PTE_USER) kernel_page_dir[dir_index] |= PTE_USER;
}

pte_t *pt = (pte_t *)(kernel_page_dir[dir_index] & PAGE_MASK);
pt[table_index] = (phys & PAGE_MASK) | flags | PTE_PRESENT;
```

#### Paging_create_user_dir (process isolation)

1. alloc physical page for page directory via `PMM_alloc_page()`
2. temporary map at 0x00300000
3. clone each pde from kernel_page_dir:
 - pde index 0 (identity map): copy without PTE_USER
 - pde index 0xC00-0xC03 (higher-half): copy without PTE_USER
 - pde other: copy with PTE_USER
4. each clone using page table fisik new from `PMM_alloc_page()`
5. return physical address

#### Paging_switch_dir(dir)

```c
uint32_t pd_phys;
if (dir == NULL) pd_phys = (uint32_t)kernel_page_dir;
else pd_phys = (uint32_t)dir;
asm volatile("mov %0, %%cr3" : : "r"(pd_phys));
```

### Heap allocator (kmalloc/kfree)

file: `src/kernel/lib/heap.c`

#### Block header

```c
typedef struct heap_block {
 uint32_t size; /* total ukuran block (header + payload) */
 uint16_t magic; /* MAGIC_FREE (0xF4EE) or MAGIC_USED (0x1CED) */
 uint16_t flags; /* bit 0: free */
 struct heap_block *next; /* next free block */
 struct heap_block *prev; /* prev free block */
} heap_block_t;
```

sizeof = .

#### Kmalloc(size)

1. `needed = align_up(sizeof(header) + size)`, minimal payload
2. first-fit scan free list
3. if block enough gede (`>= needed + block_MIN`): split
4. if not ketemu: expand heap (alloc page via PMM), coalesce with last block if adjacent

#### Kfree(ptr)

1. validasi magic number
2. add to the free list (sorted by address)
3. coalesce with adjacent free blocks

#### Splitting

```text
block A (size 1000) -> malloc(32) -> needed = align_up(16+32) = 48
block A di-split:
 [used A size=48] [free A2 size=952]
```

#### Coalescing

```text
[free A size 48] [free B size 952 adjacent in memory]
-> after coalescing: [free C size 1000]
```

#### Expand_heap

```c
int npages = expand_heap(0x10000); // 64kb initial
// allocate a physical page and map it at HEAP_BASE + old_committed * 0x1000
committed_pages += npages;
```

## Task scheduler

file: `src/kernel/tasks/task.c`

### Structure task

```c
typedef struct task_t {
 uint32_t id;
 uint32_t ppid;
 task_state_t state; // READY=0, RUNNING=1, BLOCKED=2, DEAD=3
 int exit_code;
 task_context_t context; // registers + cs + eflags + cr3
 uint32_t *stack;
 uint32_t stack_base;
 struct task *parent;
 struct task *child_first;
 struct task *sibling_next;
 struct task_t *next;
 struct task_t *prev;
 fd_table_t *fd_table;
} task_t;
```

context:
```c
typedef struct {
 uint32_t eax, ebx, ecx, edx;
 uint32_t esi, edi, ebp, esp;
 uint32_t eip;
 uint32_t eflags;
 uint32_t cs;
 uint32_t cr3;
} task_context_t;
```

### Task_create(function_pointer)

1. allocate a 16 KiB stack: `PMM_alloc_page()` -> `page_map()` at 0x10000 + task_count * 4096
2. setup task context:
 - esp = stack_base + task_STACK_size - 4
 - ebp = esp
 - eip = (uint32_t)entry
 - eflags = 0x202
 - cs = 0x08
3. init fd_table
4. link it into the circular list

### Task_switch()

```c
void task_switch(void) {
 if (task_count <= 1) return;

 task_t *prev = current_task;

 // round-robin
 current_task = current_task->next;
 if (!current_task) current_task = &tasks[0];
 current_task->state = TASK_RUNNING;

 // cr3 switching
 if (current_task->context.cr3 != 0)
 paging_switch_dir((pde_t *)current_task->context.cr3);
 else if (prev && prev->context.cr3 && !current_task->context.cr3)
 paging_switch_dir(NULL);
}
```

### Task_create_user (ring 3 task)

di `src/kernel/tasks/task_user.c`:

1. code is already assembled into CODE_VIRT (0x40000000) by asm_assemble
2. map pages at CODE_VIRT with PTE_USER (so ring 3 can access them)
3. allocate a 16 KiB stack at 0xF00000 with PTE_USER
4. setup tcb:
 - cs = USER_CS (0x1B)
 - eflags = 0x202
 - eip = code_addr (0x40000000)
 - cr3 = from paging_create_user_dir()
5. `switch_to_user()` -> iret to ring 3

### Switch_to_user()

```c
asm volatile(
 "pushl %2\n" /* ss = 0x23 */
 "pushl %1\n" /* esp */
 "pushl %3\n" /* eflags = 0x202 */
 "pushl %0\n" /* cs = 0x1B */
 "pushl %4\n" /* eip */
 "mov %2, %%eax\n"
 "mov %%eax, %%ds\n"
 "mov %%eax, %%es\n"
 "mov %%eax, %%fs\n"
 "mov %%eax, %%gs\n"
 "iret\n"
);
```

## Klilbc -- kernel libc

file: `src/kernel/lib/klibc.c`

function standard c that runs in kernel mode, using vga_putc/vga_print directly.

### Printf

```c
int klibc_printf(const char *fmt, ...) {
 va_list args;
 va_start(args, fmt);

 for (int i = 0; fmt[i]; i++) {
 if (fmt[i] != '%') { vga_putc(fmt[i]); count++; continue; }
 i++;
 switch (fmt[i]) {
 case 'd': count += print_int(va_arg(args, int)); break;
 case 's': count += print_padded_string(va_arg(args, char*), width, 0, left); break;
 case 'x': count += print_unsigned(va_arg(args, unsigned), 16, 0); break;
 case 'u': count += print_unsigned(va_arg(args, unsigned), 10, 0); break;
 case 'o': count += print_unsigned(va_arg(args, unsigned), 8, 0); break;
 case 'p': vga_print("0x"); count += print_unsigned((uint32_t)va_arg(args,void*),16,0); break;
 case 'c': vga_putc(va_arg(args, int)); count++; break;
 case '%': vga_putc('%'); count++; break;
 }
 }
 va_end(args);
}
```

### Scanf

```c
int klibc_scanf(const char *fmt, ...) {
 gets(line); // read multiple lines from the keyboard
 for (int i = 0; fmt[i]; i++) {
 if (fmt[i] != '%') {
 if (fmt[i] == ' ' || fmt[i] == '\t') continue;
 if (line[lpos] == fmt[i]) lpos++;
 else break;
 continue;
 }
 i++;
 switch (fmt[i]) {
 case 'd': /* parse integer desimal */
 case 's': /* parse string sampe whitespace */
 case 'x': /* parse hex */
 case 'o': /* parse octal */
 }
 }
}
```

### Sprintf

```c
int klibc_sprintf(char *buf, const char *fmt, ...) {
 // like printf, but writes to a buffer instead of VGA
 va_start(args, fmt);
 for (int i = 0; fmt[i]; i++) {
 if (fmt[i] != '%') { buf[pos++] = fmt[i]; continue; }
 i++;
 switch (fmt[i]) {
 case 'd': itoa(val, tmp, 10); for(...) buf[pos++] = tmp[j]; break;
 case 's': while(*s) buf[pos++] = *s++; break;
 }
 }
 buf[pos] = '\0';
}
```

### Atoi

```c
int klibc_atoi(const char *s) {
 int sign = 1, num = 0;
 while (*s == ' ' || *s == '\t') s++;
 if (*s == '-') { sign = -1; s++; }
 while (*s >= '0' && *s <= '9') { num = num * 10 + (*s - '0'); s++; }
 return num * sign;
}
```

## Usrlib -- user space library

file: `src/kernel/lib/usrlib.c`

function that using int 0xsyscall, bukan directly hardware. aman called from ring 3.

### Syscall wrappers

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

### Usr_printf

```c
int usr_printf(const char *fmt, ...) {
 for (int i = 0; fmt[i]; i++) {
 if (fmt[i] != '%') { usr_putchar(fmt[i]); continue; }
 i++;
 switch (fmt[i]) {
 case 'd': /* convert int to string -> write via syscall */
 case 's': /* write string via syscall */
 case 'x': /* hex via syscall */
 }
 }
}
```

each putchar using `sys3(SYSCALL_WRITE_FD, 1, &ch, 1)`.

### Usr_malloc

using brk syscall:
```c
void *usr_malloc(uint32_t size) {
 uint32_t cur = sys1(SYSCALL_BRK, 0);
 if (cur == 0) return 0;
 uint32_t new = sys1(SYSCALL_BRK, cur + size + 4);
 if (new <= cur) return 0;
 return (void *)(cur + 4);
}
```

## Logging

file: `src/kernel/lib/log.c`

### Circular buffer

```c
static char log_buf[LOG_BUF_SIZE]; // 
static volatile int write_pos = 0;
static volatile int read_pos = 0;
```

### Log_printf

format: `[timestamp] message\n`

```c
void log_printf(const char *fmt, ...) {
 // write timestamp
 log_putchar('[');
 log_int(timer_get_ticks(), 10);
 log_puts("] ");

 va_start(args, fmt);
 for (int i = 0; fmt[i]; i++) {
 if (fmt[i] != '%') { log_putchar(fmt[i]); continue; }
 i++;
 switch (fmt[i]) {
 case 's': log_puts(va_arg(args, char*)); break;
 case 'd': log_int(va_arg(args, int), 10); break;
 case 'x': log_hex(va_arg(args, uint32_t)); break;
 }
 }
 log_putchar('\n');
}
```

overflow: if write_pos == read_pos, read_pos maju (data old di-overwrite).

### Log_exception

called from interrupt_handler:
```c
log_printf("[EXC] int=%d err=0x%x cr2=0x%x eip=0x%x", int_num, err_code, cr2_val, eip_val);
```

### Log_dump (dmesg)

print all isi circular buffer to VGA. from read_pos sampe write_pos.
