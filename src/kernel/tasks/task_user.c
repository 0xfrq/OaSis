/*
 * task_user.c - User mode task management
 */

#include "task_user.h"
#include "pmm.h"
#include "paging.h"
#include "vga.h"
#include "string.h"
#include "gdt.h"
#include "asm.h"
#include "syscall.h"
#include "fd.h"
#include "keyboard.h"
#include <stddef.h>

#define USER_CODE_BASE  0x00800000
#define USER_STACK_BASE 0x00F00000
#define USER_STACK_SIZE 0x4000

static uint32_t next_user_code = USER_CODE_BASE;
static uint32_t next_user_stack = USER_STACK_BASE;

__attribute__((noinline))
void switch_to_user(uint32_t eip, uint32_t esp) {
    uint32_t eflags = 0x202;

    asm volatile(
        "cli\n"
        "pushl %2\n"      /* SS  = 0x23 */
        "pushl %1\n"      /* ESP */
        "pushl %3\n"      /* EFLAGS */
        "pushl %0\n"      /* CS  = 0x1B */
        "pushl %4\n"      /* EIP */
        "mov %2, %%eax\n"
        "mov %%eax, %%ds\n"
        "mov %%eax, %%es\n"
        "mov %%eax, %%fs\n"
        "mov %%eax, %%gs\n"
        "iret\n"
        :
        : "r"(USER_CS),
          "r"(esp),
          "r"(USER_DS),
          "r"(eflags),
          "r"(eip)
        : "eax", "memory"
    );
    while (1);
}

task_t *task_create_user(const char *name, void *code_start, uint32_t code_size) {
    (void)name;

    /* Use code_start directly (already at CODE_VIRT = 0x40000000).
     * Map the physical pages backing CODE_VIRT as PTE_USER so user can access them.
     * This avoids the problem of absolute address references (like buf) being wrong. */
    uint32_t code_addr = (uint32_t)code_start;
    uint32_t code_pages = (code_size + 0xFFF) / 0x1000;
    if (code_pages < 1) code_pages = 1;

    /* Map 4 pages (16KB) di CODE_VIRT biar muat code + data besar */
    for (uint32_t i = 0; i < 4; i++) {
        uint32_t page_virt = code_addr + i * 0x1000;
        uint32_t pde_idx = page_virt >> 22;
        uint32_t pte_idx = (page_virt >> 12) & 0x3FF;

        if ((kernel_page_dir[pde_idx] & PTE_PRESENT)) {
            pte_t *pt = (pte_t *)(kernel_page_dir[pde_idx] & PAGE_MASK);
            if (!(pt[pte_idx] & PTE_PRESENT)) {
                uint32_t phys = pmm_alloc_page();
                if (!phys) return NULL;
                pt[pte_idx] = phys | PTE_PRESENT | PTE_WRITE | PTE_USER;
            } else {
                pt[pte_idx] |= PTE_USER;
            }
            kernel_page_dir[pde_idx] |= PTE_USER;
        } else {
            uint32_t phys = pmm_alloc_page();
            if (!phys) return NULL;
            page_map(page_virt, phys, PTE_PRESENT | PTE_WRITE | PTE_USER);
        }
        asm volatile("invlpg (%0)" : : "r"(page_virt) : "memory");
    }

    uint32_t stack_addr = next_user_stack + USER_STACK_SIZE - 4;
    for (uint32_t i = 0; i < USER_STACK_SIZE / 0x1000; i++) {
        uint32_t phys = pmm_alloc_page();
        if (phys == 0) return NULL;
        page_map((next_user_stack) + i * 0x1000, phys, PTE_PRESENT | PTE_WRITE | PTE_USER);
        asm volatile("invlpg (%0)" : : "r"(next_user_stack + i * 0x1000) : "memory");
    }

    /* Stub: int 0x80; jmp -2 — ditempatkan di akhir kode user */
    uint8_t exit_stub[4] = { 0xCD, 0x80, 0xEB, 0xFE };
    uint32_t stub_addr = code_addr + code_size;
    /* Map additional page if needed for stub */
    uint32_t stub_page = stub_addr & ~0xFFF;
    if (stub_page > code_addr && !(kernel_page_dir[stub_page >> 22] & PTE_PRESENT)) {
        /* Page already mapped, just copy */
    }
    memcpy((void *)stub_addr, exit_stub, sizeof(exit_stub));

    *(uint32_t *)(next_user_stack + USER_STACK_SIZE - 4) = stub_addr;

    extern task_t tasks[];
    extern int task_count;
    if (task_count >= TASK_MAX) return NULL;

    task_t *task = &tasks[task_count];
    task->id = task_count + 1;
    task->state = TASK_READY;
    task->ppid = 0;
    task->exit_code = 0;
    task->parent = NULL;
    task->child_first = NULL;
    /* Allocate fd_table for user task */
    { static fd_table_t user_fd; fd_table_init(&user_fd); task->fd_table = &user_fd; }
    task->stack = (uint32_t *)next_user_stack;
    task->stack_base = next_user_stack;

    if (task_count > 0) {
        tasks[task_count - 1].next = task;
        task->prev = &tasks[task_count - 1];
    }
    task->next = &tasks[0];

    task->context.eip = code_addr;
    task->context.esp = stack_addr;
    task->context.eflags = 0x202;
    task->context.cs = USER_CS;
    task->context.ebp = 0;
    task->context.ebx = 0;
    task->context.ecx = 0;
    task->context.edx = 0;
    task->context.esi = 0;
    task->context.edi = 0;
    task->context.eax = 0;

    task_count++;

    next_user_code  = code_addr + code_pages * 0x1000;
    next_user_stack = next_user_stack + USER_STACK_SIZE;

    char buf[16];
    vga_print("[+] User task ID=");
    itoa(task->id, buf, 10);
    vga_print(buf);
    vga_print(" Code=0x");
    itoa(code_addr, buf, 16);
    vga_print(buf);
    vga_print("\n");

    return task;
}

void run_user_test(const char *asm_code) {
    void *exec_addr = 0;
    int result = asm_assemble(asm_code, &exec_addr);
    if (result <= 0) {
        vga_print("user: assembling gagal\n");
        return;
    }

    task_t *task = task_create_user("usertest", exec_addr, (uint32_t)result);
    if (!task) {
        vga_print("user: gagal bikin task\n");
        return;
    }

    extern volatile uint32_t user_exit_eip;
    extern volatile uint32_t user_exit_esp;
    extern volatile int user_task_active;

    /* Simpan alamat balik di kernel stack */
    extern void user_return_to_shell(void);
    user_exit_eip = (uint32_t)user_return_to_shell;
    asm volatile("mov %%ebp, %0" : "=r"(user_exit_esp));

    /* Bikin page directory khusus buat user task (process isolation) */
    extern pde_t *paging_create_user_dir(void);
    extern void paging_switch_dir(pde_t *dir);
    pde_t *user_pd = paging_create_user_dir();
    if (user_pd) {
        task->context.cr3 = (uint32_t)user_pd;
        paging_switch_dir(user_pd);
        vga_print("[!] User page directory active\n");
    }

    user_task_active = 1;
    vga_print("user: switching to ring 3...\n");
    switch_to_user(task->context.eip, task->context.esp);
    /* Gak pernah sampe sini */
}
