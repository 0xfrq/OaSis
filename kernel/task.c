#include "task.h"
#include "vga.h"
#include "string.h"
#include "pmm.h"
#include "paging.h"
#include "fd.h"
#include <stddef.h>

// Global task array
task_t tasks[TASK_MAX];
static int task_count = 0;
static int current_task_id = 0;
task_t *current_task = NULL;

/* Per-task file descriptor tables (static allocation for simplicity) */
static fd_table_t task_fd_tables[TASK_MAX];

void task_init(void) {
    vga_print("[*] Initializing task manager...\n");
    
    for (int i = 0; i < TASK_MAX; i++) {
        tasks[i].id = 0;
        tasks[i].state = TASK_DEAD;
        tasks[i].next = NULL;
        tasks[i].prev = NULL;
        tasks[i].fd_table = NULL;
    }
    
    vga_print("[+] Task manager initialized\n");
}

task_t *task_create(void (*entry)(void)) {
    vga_print("[DEBUG] task_create called\n");
    
    if (task_count >= TASK_MAX) {
        vga_print("ERROR: Task limit reached\n");
        return NULL;
    }
    
    vga_print("[DEBUG] task_count = ");
    char buf[16];
    itoa(task_count, buf, 10);
    vga_print(buf);
    vga_print(", creating task\n");
    
    task_t *task = &tasks[task_count];
    task->id = ++current_task_id;
    task->state = TASK_READY;
    task->ppid = 0;
    task->exit_code = 0;
    task->parent = NULL;
    task->child_first = NULL;
    
    /* Day 10: Initialize file descriptor table */
    task->fd_table = &task_fd_tables[task_count];
    fd_table_init(task->fd_table);
    
    // For now, skip physical memory allocation - just use a simple virtual address
    // In production, would use pmm_alloc_page() and page_map()
    uint32_t stack_virt = 0x10000 + (task_count * TASK_STACK_SIZE);
    
    task->stack = (uint32_t *)stack_virt;
    task->stack_base = stack_virt;
    
    // Initialize context
    task->context.esp = stack_virt + TASK_STACK_SIZE - 4;
    task->context.ebp = task->context.esp;
    task->context.eip = (uint32_t)entry;
    task->context.eflags = 0x202;
    task->context.cs = 0x08;
    
    // Initialize other registers
    task->context.edi = 0;
    task->context.esi = 0;
    task->context.ebx = 0;
    task->context.edx = 0;
    task->context.ecx = 0;
    task->context.eax = 0;
    
    // Link task into ready queue
    if (task_count > 0) {
        tasks[task_count - 1].next = task;
        task->prev = &tasks[task_count - 1];
    }
    task->next = &tasks[0];
    
    task_count++;
    
    vga_print("[+] Task created: ID=");
    itoa(task->id, buf, 10);
    vga_print(buf);
    vga_print(" Stack=0x");
    itoa(stack_virt, buf, 16);
    vga_print(buf);
    vga_print("\n");
    
    return task;
}

void task_yield(void) {
    // Trigger scheduler on next timer interrupt
    // (This will be called from tasks)
}

void task_switch(void) {
    if (task_count == 0) return;
    if (task_count == 1) return;  // Only one task, don't switch
    
    // Move to next task in round-robin
    if (current_task == NULL) {
        current_task = &tasks[0];
    } else {
        current_task = current_task->next;
        if (current_task == NULL) {
            current_task = &tasks[0];
        }
    }
    
    current_task->state = TASK_RUNNING;
}

task_t *task_get_current(void) {
    return current_task;
}

task_t *get_task_ptr(int id) {
    if (id < 0 || id >= TASK_MAX) return NULL;
    return &tasks[id];
}

void task_print_info(void) {
    vga_print("Task Info:\n");
    char buf[16];
    
    // Debug: print actual task_count variable
    vga_print("  Total tasks: ");
    itoa(task_count, buf, 10);
    vga_print(buf);
    vga_print(" (internal count)\n");
    
    // Also print how many tasks have non-zero IDs
    int real_count = 0;
    for (int i = 0; i < TASK_MAX; i++) {
        if (tasks[i].id != 0) real_count++;
    }
    vga_print("  Real tasks found: ");
    itoa(real_count, buf, 10);
    vga_print(buf);
    vga_print("\n\n");
    
    for (int i = 0; i < task_count && i < TASK_MAX; i++) {
        task_t *t = &tasks[i];
        if (t->id == 0) break;  // Stop at empty slot
        
        vga_print("  Task ");
        itoa(i, buf, 10);
        vga_print(buf);
        vga_print(": ID=");
        itoa(t->id, buf, 10);
        vga_print(buf);
        vga_print(" State=");
        
        switch (t->state) {
            case TASK_READY: vga_print("READY"); break;
            case TASK_RUNNING: vga_print("RUNNING"); break;
            case TASK_BLOCKED: vga_print("BLOCKED"); break;
            case TASK_DEAD: vga_print("DEAD"); break;
            default: vga_print("UNKNOWN"); break;
        }
        
        vga_print(" Stack=0x");
        itoa(t->stack_base, buf, 16);
        vga_print(buf);
        vga_print(" EIP=0x");
        itoa(t->context.eip, buf, 16);
        vga_print(buf);
        vga_print("\n");
    }
}


task_t *get_next_task(void) {
    if (task_count == 0) return NULL;
    if (task_count == 1) return current_task;

    if(current_task == NULL) {
        current_task = &tasks[0];
    } else {
        if(current_task->next != NULL) {
            current_task =current_task->next;
        } else {
            current_task = &tasks[0];
        }
    }
    if(current_task != NULL) {
        current_task->state = TASK_RUNNING;
    }

    return current_task;
}

int task_fork(void) {
    if (task_count >= TASK_MAX) {
        return -1;
    }

    task_t *parent = current_task;
    if(parent == NULL) return -1;

    task_t *child = &tasks[task_count];
    child->id = ++current_task_id;
    child->state = TASK_READY;
    child->ppid = parent->id;
    child->exit_code = 0;
    child->parent = parent;
    child->child_first = NULL;

    child->context = parent->context;

    child->stack_base = 0x10000 + (task_count * TASK_STACK_SIZE);
    child->stack = (uint32_t *)child->stack_base;
    child->context.esp = child->stack_base + TASK_STACK_SIZE - 4;

    /* Day 10: Copy parent's file descriptor table to child */
    child->fd_table = &task_fd_tables[task_count];
    if (parent->fd_table) {
        fd_table_copy(child->fd_table, parent->fd_table);
    } else {
        fd_table_init(child->fd_table);
    }

    if(task_count>0) {
        tasks[task_count - 1].next = child;
        child->prev = &tasks[task_count- 1];
    }
    child->next = &tasks[0];

    task_count++;

    return child->id;
}

int task_exec(const char *program, uint32_t size) {
    if (current_task == NULL) return -1;
    
    (void)program;
    (void)size;
    
    current_task->context.esp = current_task->stack_base + TASK_STACK_SIZE - 4;
    current_task->context.ebp = current_task->context.esp;
    
    current_task->context.eax = 0;
    current_task->context.ebx = 0;
    current_task->context.ecx = 0;
    current_task->context.edx = 0;
    current_task->context.esi = 0;
    current_task->context.edi = 0;
    
    return 0;
}

// Exec program: alias for task_exec
void task_exec_program(const char *program, uint32_t size) {
    task_exec(program, size);
}
int task_wait(int *status) {
    if(current_task == NULL) return -1;

    task_t *child = NULL;
    for(int i=0; i<task_count; i++) {
        if (tasks[i].ppid == current_task->id && tasks[i].state != TASK_DEAD) {
            child = &tasks[i];
            break;
        }
    }

    if(child == NULL) return -1;

    if (status != NULL) {
        *status = child->exit_code;
    }

    return child->id;  

}

int task_get_parent_id(void) {
    if(current_task == NULL) return -1;
    return current_task->ppid;
}