# 04. kernel

dokumentasi ini ngebahas inti dari OaSis: kernel dan semua subsystem-nya.

## daftar isi

- [overview](#overview)
- [memory management](memory.md)
- [interrupt handling](interrupt.md)
- [task scheduling](task.md)
- [system calls](syscall.md)

---

## overview

kernel OaSis punya 4 tanggung jawab utama:

1. **memory management** - ngatur physical dan virtual memory
2. **interrupt handling** - handle hardware dan software interrupt
3. **task scheduling** - ngatur multiple process/task
4. **system call interface** - API buat aplikasi

### file utama

- `src/kernel/main.c` - entry point dan inisialisasi
- `src/kernel/memory.c` - memory management
- `src/kernel/idt.c` - interrupt descriptor table
- `src/kernel/task.c` - task scheduler
- `src/kernel/syscall.c` - system call handler

## memory management

OaSis pake 2 layer memory management:

### 1. physical memory manager (pmm)

ngatur physical memory (RAM) dalam bentuk page (4 KB).

**fitur:**
- bitmap-based allocation
- alloc dan free physical pages
- tracking free memory

**api:**
```c
void pmm_init(void);
void *pmm_alloc_page(void);
void pmm_free_page(void *addr);
uint32_t pmm_get_free_pages(void);
```

detail ada di [memory.md](memory.md)

### 2. virtual memory manager (vmm)

**status:** belum diimplementasi

**planned features:**
- paging (4 KB pages)
- virtual address space per process
- page fault handling

## interrupt handling

OaSis handle interrupt lewat **idt (interrupt descriptor table)**.

### jenis interrupt

1. **hardware interrupt** - dari device (keyboard, timer, dll)
2. **software interrupt** - dari aplikasi (system call)
3. **exception** - error (page fault, divide by zero, dll)

### setup idt

```c
void idt_init(void) {
    // setup 256 interrupt vectors
    idt_set_gate(0, (uint32_t)isr0, 0x08, 0x8E);
    idt_set_gate(1, (uint32_t)isr1, 0x08, 0x8E);
    // ... sampai 255
    
    // load idt
    idt_load();
}
```

### interrupt flow

```
interrupt terjadi
  ↓
cpu push ke stack (eip, cs, eflags)
  ↓
cpu lompat ke handler (via idt)
  ↓
handler save registers
  ↓
handler proses interrupt
  ↓
handler restore registers
  ↓
iret (return dari interrupt)
```

detail ada di [interrupt.md](interrupt.md)

## task scheduling

OaSis support **preemptive multitasking** dengan round-robin scheduler.

### konsep task

**task** = unit of execution (mirip process/thread)

**struktur task:**
```c
typedef struct {
    uint32_t id;
    uint32_t state;        // ready, running, blocked
    registers_t regs;      // saved registers
    uint32_t *stack;       // kernel stack
    // ...
} task_t;
```

### scheduler

scheduler jalan tiap timer interrupt (setiap 10 ms):

```c
void scheduler(void) {
    // save current task context
    save_context(current_task);
    
    // pick next task (round-robin)
    current_task = next_task(current_task);
    
    // restore next task context
    restore_context(current_task);
}
```

### task states

- **ready** - siap jalan, nunggu giliran
- **running** - lagi jalan
- **blocked** - nunggu sesuatu (i/o, sleep, dll)
- **dead** - udah selesai

detail ada di [task.md](task.md)

## system calls

system call adalah cara aplikasi minta service dari kernel.

### mekanisme

OaSis pake **interrupt 0x80** buat system call:

```asm
; aplikasi call syscall
mov eax, 1          ; syscall number (write)
mov ebx, 1          ; fd (stdout)
mov ecx, message    ; buffer
mov edx, len        ; length
int 0x80            ; trigger syscall
```

### syscall table

| no | nama | deskripsi |
|----|------|-----------|
| 1  | write | tulis ke file/fd |
| 2  | read | baca dari file/fd |
| 3  | open | buka file |
| 4  | close | tutup file |
| 5  | exit | keluar dari task |
| 6  | fork | duplicate task |
| ... | ... | ... |

### syscall handler

```c
void syscall_handler(registers_t *regs) {
    switch (regs->eax) {
        case 1:  // write
            regs->eax = sys_write(regs->ebx, regs->ecx, regs->edx);
            break;
        case 2:  // read
            regs->eax = sys_read(regs->ebx, regs->ecx, regs->edx);
            break;
        // ...
    }
}
```

detail ada di [syscall.md](syscall.md)

---

## file terkait

- `src/kernel/main.c` - kernel entry point
- `src/kernel/memory.c` - memory manager
- `src/kernel/idt.c` - interrupt handling
- `src/kernel/task.c` - scheduler
- `src/kernel/syscall.c` - system call interface

selanjutnya: [driver →](../05-driver/readme.md)
