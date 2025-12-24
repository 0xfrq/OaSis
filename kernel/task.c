#include "task.h"
#include "vga.h"
#include "string.h"
#include "pmm.h"
#include "paging.h"
#include <stddef.h>

static task_t tasks[TASK_MAX];
static int task_count = 0;
static int current_task_id = 0;

task_t *current_task = NULL;

void task_init(void) {
    vga_print("[*] Initializing task manager..");

    for(int i=0; i<TASK_MAX; i++) {
        tasks[i].id = 0;
        tasks[i].state = TASK_DEAD;
        tasks[i].next = NULL;
        tasks[i].prev = NULL;
    }

    vga_print("[*] Task Manager initialized");
}

task_t *task_create(void(*entry)(void)) {
    if(task_count >= TASK_MAX) {
        vga_print("ERROR: Task limit reached, close dulu kek beberapa\n");
        return NULL;
    }

    task_t *task = &tasks[task_count];
    task->id = ++current_task_id;
    task->state = TASK_READY;

    uint32_t stack_phys = pmm_alloc_page();
    if(!stack_phys) {
        vga_print("ERROR: Cannot allocate task stack\n");
        return NULL;
    }

    uint32_t stack_virt = 0x10000 + (task_count * TASK_STACK_SIZE);
    page_map(stack_virt, stack_phys, 0x03);

    task->stack = (uint32_t *)stack_virt;
    task->stack_base = stack_virt;

    task->context.esp = stack_virt + TASK_STACK_SIZE - 4;
    task->context.ebp = task->context.esp;
    task->context.eip = (uint32_t)entry;
    task->context.eflags = 0x202;
    task->context.cs = 0x08;

    if(task_count > 0) {
        tasks[task_count - 1].next = task;
        task->prev = &tasks[task_count - 1];
    }

    task->next = &tasks[0];

    task_count++;

    char buf[16];
    vga_print("[+] Task created: ID = ");
    itoa(task->id, buf, 10);
    vga_print(buf);
    vga_print(" Stack=0x");
    itoa(stack_virt, buf, 16);
    vga_print(buf);
    vga_print("\n");

    return task;
}

void task_yield(void) {

}

void task_switch(void) {
    if (task_count == 0) return;
    if (task_count == 1) return;

    if (current_task == NULL) {
        current_task = &tasks[0];
    } else {
        current_task = current_task->next;
        if(current_task == NULL) {
            current_task = &tasks[0];
        }
    }

    current_task->state = TASK_RUNNING;
}

task_t *task_get_current(void) {
    return current_task;
}

void task_print_info(void) {
    vga_print("Task Info:\n");
    char buf[16];

    vga_print("     Total tasks: ");
    itoa(task_count, buf, 10);
    vga_print(buf);
    vga_print("\n");

    for (int i = 0; i<task_count; i++){
        task_t *t = &tasks[i];
        vga_print("     Task ");
        itoa(t->id, buf, 10);
        vga_print(buf);
        vga_print(": ID = ");
        itoa(t->id, buf, 10);
        vga_print(buf);
        vga_print(" State = ");

        switch (t->state) {
            case TASK_READY: vga_print("READY"); break;
            case TASK_RUNNING: vga_print("RUNNING"); break;
            case TASK_BLOCKED: vga_print("BLOCKED"); break;
            case TASK_DEAD: vga_print("DEAD"); break;

        }
        
        vga_print(" EIP=0x");
        itoa(t->context.eip, buf, 16);
        vga_print(buf);
        vga_print("\n");
    }
}