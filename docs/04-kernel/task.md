---
layout: default
title: Task Scheduling
---

# Task Scheduling

## Task Structure

Each task is represented by a `task_t` structure:

```c
typedef struct task_t {
    uint32_t id;             // Task ID (1-based)
    uint32_t ppid;           // Parent PID
    task_state_t state;      // READY, RUNNING, BLOCKED, DEAD
    int exit_code;           // Exit status
    task_context_t context;  // Registers + EIP + CR3
    uint32_t *stack;         // Stack pointer
    uint32_t stack_base;     // Stack base address
    struct task_t *next;     // Next task in circular list
    struct task_t *prev;     // Prev task in circular list
    fd_table_t *fd_table;    // File descriptor table
} task_t;
```

## Task Creation

`task_create(function_pointer)` allocates a stack (4 pages = 16KB), maps it as user pages, initializes the context (EIP = function address, EFLAGS = 0x202, CS = 0x08), and adds the task to the circular ready list.

### Stack Layout per Task
```text
Stack base + 16KB
  [return address for first function]
  [... unused ...]
Stack base
  4KB page (physical)
```

## Scheduler Algorithm

The scheduler uses **round-robin** with a circular linked list:

1. Timer IRQ fires at 100Hz (every 10ms).
2. `timer_interrupt_handler()` increments tick counter and calls `task_switch()`.
3. `task_switch()` advances `current_task = current_task->next`.
4. Updates CR3 if the new task has a different page directory.
5. Returns to the interrupted context (no actual register save/restore — uses the IRET frame already on the stack).

## Task States

- `TASK_READY` (1): Can be scheduled.
- `TASK_RUNNING` (2): Currently executing.
- `TASK_BLOCKED` (3): Waiting for an event.
- `TASK_DEAD` (4): Terminated, waiting for cleanup.

## User Mode Tasks

`task_create_user()` in `task_user.c` creates a ring 3 task:
1. Takes assembled code from CODE_VIRT (0x40000000).
2. Maps 4 pages at CODE_VIRT with PTE_USER.
3. Allocates stack pages at 0xF00000.
4. Sets up TCB with CS = 0x1B (USER_CS), DS = 0x23 (USER_DS).
5. Switches to user mode via `switch_to_user()` which builds an iret frame with ring 3 selectors.

## CR3 Switching

When switching between tasks with different page directories, CR3 is updated:
```c
if (current_task->context.cr3 != 0) {
    paging_switch_dir((pde_t *)current_task->context.cr3);
}
```

The `user_return_to_shell` stub restores kernel CR3 on user exit.
