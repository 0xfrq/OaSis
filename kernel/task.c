#include "task.h"
#include "vga.h"
#include "string.h"
#include "pmm.h"
#include "paging.h"
#include <stddef.h>

// Global task array
task_t tasks[TASK_MAX];
static int task_count = 0;
static int current_task_id = 0;
task_t *current_task = NULL;

void task_init(void) {
    vga_print("[*] Initializing task manager...\n");
    
    for (int i = 0; i < TASK_MAX; i++) {
        tasks[i].id = 0;
        tasks[i].state = TASK_DEAD;
        tasks[i].next = NULL;
        tasks[i].prev = NULL;
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
