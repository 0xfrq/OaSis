# task scheduling

dokumentasi ini ngebahas gimana OaSis ngatur multiple task/process.

## daftar isi

- [konsep task](#konsep-task)
- [task structure](#task-structure)
- [task states](#task-states)
- [scheduler](#scheduler)
- [context switching](#context-switching)
- [api reference](#api-reference)

---

## konsep task

**task** adalah unit of execution di OaSis. mirip sama process/thread di OS modern.

### karakteristik task

- jalan di kernel mode (ring 0)
- punya stack sendiri
- punya saved registers
- di-schedule secara preemptive

**catatan:** OaSis belum support user mode, jadi semua task jalan di kernel.

## task structure

```c
typedef struct task {
    uint32_t id;              // task id
    uint32_t state;           // ready, running, blocked, dead
    registers_t regs;         // saved registers (eip, esp, dll)
    uint32_t *stack;          // kernel stack
    uint32_t stack_size;      // stack size
    uint32_t sleep_until;     // tick buat wake up (kalo sleeping)
    struct task *next;        // pointer ke task berikutnya (linked list)
} task_t;
```

### registers_t

```c
typedef struct {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  // general purpose
    uint32_t eip;  // instruction pointer
    uint32_t eflags;  // flags
} registers_t;
```

## task states

task bisa ada di 4 state:

```
    ┌──────────┐
    │  ready   │◄──────┐
    └────┬─────┘       │
         │ schedule    │ time slice habis
         ▼             │
    ┌──────────┐       │
    │ running  │───────┘
    └────┬─────┘
         │ block (wait i/o, sleep)
         ▼
    ┌──────────┐
    │ blocked  │─────► ready (kalo event selesai)
    └────┬─────┘
         │ exit
         ▼
    ┌──────────┐
    │   dead   │
    └──────────┘
```

### state descriptions

**ready**
- siap jalan
- nunggu giliran di schedule
- ada di ready queue

**running**
- lagi jalan di cpu
- cuma 1 task yang running di satu waktu

**blocked**
- nunggu sesuatu (i/o, sleep, dll)
- gak bakal di-schedule sampe event selesai
- contoh: nunggu keyboard input, sleeping

**dead**
- udah selesai (exit)
- tinggal di-cleanup
- resources di-free

## scheduler

OaSis pake **preemptive round-robin scheduler**.

### preemptive

- task bisa di-interrupt di tengah jalan
- scheduler dipanggil tiap timer interrupt (10 ms)
- task dapet time slice yang sama

### round-robin

- semua ready task di-schedule secara bergilir
- fair scheduling (semua task dapet giliran)
- simple dan predictable

### algoritma

```c
void schedule(void) {
    // 1. save current task context
    save_context(current_task);
    
    // 2. update current task state
    if (current_task->state == RUNNING) {
        current_task->state = READY;
    }
    
    // 3. find next ready task (round-robin)
    task_t *next = current_task->next;
    while (next != current_task) {
        if (next->state == READY) {
            break;
        }
        next = next->next;
    }
    
    // 4. switch to next task
    if (next != current_task && next->state == READY) {
        current_task = next;
        current_task->state = RUNNING;
        restore_context(current_task);
    } else {
        // no other ready task, continue current
        current_task->state = RUNNING;
        restore_context(current_task);
    }
}
```

### kapan scheduler dipanggil?

1. **timer interrupt** - tiap 10 ms (preemptive)
2. **task block** - pas task nunggu sesuatu
3. **task exit** - pas task selesai
4. **task yield** - task voluntary give up cpu

## context switching

**context switch** adalah proses switch dari satu task ke task lain.

### apa yang di-save?

semua register cpu:
- general purpose registers (eax, ebx, ecx, edx, esi, edi, ebp, esp)
- instruction pointer (eip)
- flags (eflags)

### save context

```asm
save_context:
    ; save general purpose registers
    pusha
    
    ; save eflags
    pushf
    
    ; save esp ke task structure
    mov eax, [current_task]
    mov [eax + task.esp], esp
    
    ret
```

### restore context

```asm
restore_context:
    ; load esp dari task structure
    mov eax, [current_task]
    mov esp, [eax + task.esp]
    
    ; restore eflags
    popf
    
    ; restore general purpose registers
    popa
    
    ret
```

### context switch flow

```
task A running
  ↓
timer interrupt
  ↓
save task A context (registers ke task A structure)
  ↓
scheduler pick task B
  ↓
restore task B context (registers dari task B structure)
  ↓
task B running (lanjut dari terakhir stop)
```

## api reference

### inisialisasi

```c
void task_init(void);
```

inisialisasi task system. bikin idle task.

### create task

```c
task_t *task_create(void (*entry_point)(void));
```

bikin task baru.

**parameter:**
- `entry_point`: function yang bakal dijalanin task

**return:**
- pointer ke task (success)
- `NULL` (failed)

**contoh:**
```c
void my_task(void) {
    while (1) {
        // do something
    }
}

task_t *t = task_create(my_task);
```

### exit task

```c
void task_exit(void);
```

keluar dari task sekarang. task bakal di-mark sebagai dead.

**contoh:**
```c
void my_task(void) {
    // do something
    task_exit();  // selesai
}
```

### yield

```c
void task_yield(void);
```

voluntary give up cpu. task bakal di-schedule ulang nanti.

**contoh:**
```c
void my_task(void) {
    while (1) {
        // do something
        task_yield();  // kasih kesempatan task lain
    }
}
```

### sleep

```c
void task_sleep(uint32_t ticks);
```

sleep buat beberapa tick (1 tick = 10 ms).

**parameter:**
- `ticks`: jumlah tick buat sleep

**contoh:**
```c
task_sleep(100);  // sleep 1 second (100 * 10ms)
```

### get current task

```c
task_t *task_get_current(void);
```

dapetin task yang lagi running.

**return:** pointer ke current task

---

## contoh: multiple tasks

```c
void task_a(void) {
    while (1) {
        print("a");
        task_yield();
    }
}

void task_b(void) {
    while (1) {
        print("b");
        task_yield();
    }
}

void main(void) {
    task_create(task_a);
    task_create(task_b);
    
    while (1) {
        task_yield();
    }
}
```

output: `abababab...`

---

**kembali ke:** [kernel →](readme.md)
