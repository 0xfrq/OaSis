#include "vga.h"
#include "keyboard.h"
#include "io.h"
#include "string.h"
#include "idt.h"
#include "pic.h"
#include "timer.h"
#include "memory.h"
#include "paging.h"
#include "pmm.h"
#include "task.h"
#include "tasks_demo.h"
#include "syscall.h"


#define INPUT_MAX 128



void kernel_main(void) {
    vga_clear();
    vga_print("=== OASIS ===\n");
    vga_print("Initializing interrupt system...\n\n");

    vga_print("[*] Setting up IDT...\n");
    idt_init();

    vga_print("[*] Setting up PIC...\n");
    pic_init();

    vga_print("[*] Initializing timer (100 Hz)...\n");
    timer_init(100);
    pic_enable_irq(0);

    vga_print("[*] Initializing keyboard...\n");
    keyboard_init();
    pic_enable_irq(1);

    vga_print("[*] Enabling interrupts...\n");
    asm volatile("sti");

    vga_print("\n=== OASIS Ready ===\n");
    vga_print("Interrupts enabled\n\n");

    vga_print("[*] Initializing memory system...\n");
    
    vga_print("[*] Detecting memory (e820)...\n");
    memory_init();
    memory_print_map();

    uint32_t total_mem = memory_get_total_usable();
    vga_print("Total usable memory: ");
    char buf[16];
    itoa(total_mem / 1024 / 1024, buf, 10);
    vga_print(buf);
    vga_print("MB\n\n");

    pmm_init(total_mem);

    paging_init();
    paging_enable();

    vga_print("\n[+] Memory system initialized\n");

    vga_print("\n[*] Initializing task manager...\n");
    task_init();

    vga_print("\n[*] Initializing system calls...\n");
    syscall_init();

    vga_print("[*] Creating tasks...\n");
    vga_print("[DEBUG] About to call task_create with task_idle\n");

    task_create(task_idle);
    vga_print("[DEBUG] Returned from first task_create\n");

    task_create(task_worker);
    vga_print("[DEBUG] Returned from second task_create\n");



    vga_print("[+] Tasks created and ready\n");
    vga_print("[*] Tasks managed by scheduler (timer-driven)\n");
    vga_print("Type 'help' for commands\n\n");


    char input[INPUT_MAX];
    int index = 0;
    
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_print("oasis> ");
    vga_set_color(15, VGA_COLOR_BLACK);

    while (1) {
        char c = keyboard_getchar();

        if(c=='\n') {
            input[index] = 0;
            vga_putc('\n');

            if(strcmp(input, "help") == 0) {
                vga_print("Commands:\n");
                vga_print("  help    - show this message\n");
                vga_print("  clear   - clear screen\n");
                vga_print("  uptime  - show system uptime\n");
                vga_print("  meminfo - show memory info\n");
                vga_print("  taskinfo - show task info\n");
            } else if (strcmp(input, "clear") == 0) {
                vga_clear();
            } else if (strcmp(input, "uptime") == 0) {
                uint32_t ticks = timer_get_ticks();
                uint32_t seconds = ticks / 100;
                uint32_t minutes = seconds / 60;
                uint32_t hours = minutes / 60;
                seconds %= 60;
                minutes %= 60;
                
                char buf[16];
                vga_print("Uptime: ");
                itoa(hours, buf, 10);
                vga_print(buf);
                vga_print("h ");
                itoa(minutes, buf, 10);
                vga_print(buf);
                vga_print("m ");
                itoa(seconds, buf, 10);
                vga_print(buf);
                vga_print("s\n");
            } else if (strcmp(input, "meminfo") == 0) {
                char buf[16];
                vga_print("Physical Memory Info:\n");
                vga_print("  Free pages: ");
                itoa(pmm_get_free_pages(), buf, 10);
                vga_print(buf);
                vga_print(" (");
                itoa(pmm_get_free_pages() * 4, buf, 10);
                vga_print(buf);
                vga_print(" KB)\n");
                
                vga_print("  Total usable: ");
                itoa(memory_get_total_usable() / 1024 / 1024, buf, 10);
                vga_print(buf);
                vga_print(" MB\n");
            } else if (strcmp(input, "taskinfo") == 0) {
                task_print_info();
            } else if (index != 0) {
                vga_print("unknown command, nulis yang bener\n");
            }

            index = 0;
            vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
            vga_print("oasis> ");
            vga_set_color(15, VGA_COLOR_BLACK);
            continue;
        }

        if(c=='\b') {
            if(index > 0) {
                index--;
                vga_putc('\b');
            }
            continue;
        }
        if(index < INPUT_MAX -1) {
            input[index++] = c;
            vga_putc(c);
        }
    }
}