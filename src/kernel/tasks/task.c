#include "task.h"
#include "vga.h"
#include "string.h"
#include "pmm.h"
#include "paging.h"
#include "fd.h"
#include <stddef.h>

// Array task global
task_t tasks[TASK_MAX];
int task_count = 0;
static int current_task_id = 0;
task_t *current_task = NULL;

/* Tabel file descriptor per-task (alokasi statis biar simpel) */
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

    /* Hari 10: Inisialisasi tabel file descriptor */
    task->fd_table = &task_fd_tables[task_count];
    fd_table_init(task->fd_table);

    /* Alokasi halaman fisik buat stack task */
    uint32_t stack_virt = 0x10000 + (task_count * TASK_STACK_SIZE);

    /* Alokasi halaman fisik buat stack (4 halaman = 16KB per task) */
    for (int i = 0; i < (TASK_STACK_SIZE / PAGE_SIZE); i++) {
        uint32_t phys = pmm_alloc_page();
        if (phys == 0) {
            vga_print("ERROR: Failed to allocate stack page\n");
            return NULL;
        }
        page_map(stack_virt + (i * PAGE_SIZE), phys, PTE_PRESENT | PTE_WRITE | PTE_USER);
    }

    task->stack = (uint32_t *)stack_virt;
    task->stack_base = stack_virt;

    // Inisialisasi context
    task->context.esp = stack_virt + TASK_STACK_SIZE - 4;
    task->context.ebp = task->context.esp;
    task->context.eip = (uint32_t)entry;
    task->context.eflags = 0x202;
    task->context.cs = 0x08;

    // Inisialisasi register lainnya
    task->context.edi = 0;
    task->context.esi = 0;
    task->context.ebx = 0;
    task->context.edx = 0;
    task->context.ecx = 0;
    task->context.eax = 0;

    // Masukin task ke antrian ready
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
    // Picu scheduler di interrupt timer berikutnya
    // (Ini bakal dipanggil dari task-task)
}

void task_switch(void) {
    if (task_count == 0) return;
    if (task_count == 1) return;

    task_t *prev = current_task;

    if (current_task == NULL) {
        current_task = &tasks[0];
    } else {
        current_task = current_task->next;
        if (current_task == NULL) {
            current_task = &tasks[0];
        }
    }

    current_task->state = TASK_RUNNING;

    /* Switch CR3 kalo task punya page directory sendiri */
    if (current_task && current_task->context.cr3 != 0) {
        paging_switch_dir((pde_t *)current_task->context.cr3);
    } else if (prev && prev->context.cr3 && current_task->context.cr3 == 0) {
        paging_switch_dir(NULL);
    }
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

    // Debug: tampilin task_count yang sebenernya
    vga_print("  Total tasks: ");
    itoa(task_count, buf, 10);
    vga_print(buf);
    vga_print(" (internal count)\n");

    // Sekalian hitung ada berapa task yang ID-nya bukan nol
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
        if (t->id == 0) break;  // Berhenti di slot kosong

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

    /* Alokasi halaman fisik buat stack child */
    child->stack_base = 0x10000 + (task_count * TASK_STACK_SIZE);
    for (int i = 0; i < (TASK_STACK_SIZE / PAGE_SIZE); i++) {
        uint32_t phys = pmm_alloc_page();
        if (phys == 0) {
            vga_print("ERROR: Failed to allocate fork stack page\n");
            return -1;
        }
        page_map(child->stack_base + (i * PAGE_SIZE), phys, PTE_PRESENT | PTE_WRITE | PTE_USER);
    }
    child->stack = (uint32_t *)child->stack_base;
    child->context.esp = child->stack_base + TASK_STACK_SIZE - 4;

    /* Hari 10: Salin tabel file descriptor parent ke child */
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

// Exec program: alias buat task_exec
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
