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
#include "fd.h"
#include "tasks_io.h"
#include "block.h"
#include "ata.h"
#include "block.h"
#include "tasks_11.h"

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

    vga_print("\n[*] Initializing I/O subsystem...\n");
    fd_init();

    vga_print("\n[*] Initializing block device layer...\n");
    block_init();

    vga_print("\n[*] Initializing system calls...\n");
    syscall_init();

    vga_print("[*] Creating tasks...\n");
    vga_print("[DEBUG] About to call task_create with task_idle\n");

    task_create(task_idle);
    vga_print("[DEBUG] Returned from first task_create\n");

    task_create(task_worker);
    vga_print("[DEBUG] Returned from second task_create\n");

    task_create(task_block_test);
    vga_print("[DEBUG] Returned from block test task_create\n");

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
                vga_print("  help      - show this message\n");
                vga_print("  clear     - clear screen\n");
                vga_print("  uptime    - show system uptime\n");
                vga_print("  meminfo   - show memory info\n");
                vga_print("  taskinfo  - show task info\n");
                vga_print("  runtasks  - execute all tasks\n");
                vga_print("  iotest    - test I/O subsystem (Day 10)\n");
                vga_print("  fdinfo    - show file descriptor table\n");
                vga_print("  pipetest  - test pipe communication\n");
                vga_print("  disktest  - test disk read/write (Day 11)\n");
                vga_print("  diskinfo  - show disk/cache information\n");
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
            } else if (strcmp(input, "runtasks") == 0) {
                vga_print("\n[*] Executing tasks...\n");
                
                task_t *task1 = get_task_ptr(0);
                if (task1 && task1->id != 0) {
                    vga_print("[*] Running Task ");
                    char buf[16];
                    itoa(task1->id, buf, 10);
                    vga_print(buf);
                    vga_print(":\n");
                    
                    void (*entry_func)(void) = (void (*)(void))task1->context.eip;
                    entry_func();
                    
                    vga_print("\n[+] Task ");
                    itoa(task1->id, buf, 10);
                    vga_print(buf);
                    vga_print(" completed\n\n");
                }
                
                task_t *task2 = get_task_ptr(1);
                if (task2 && task2->id != 0) {
                    vga_print("[*] Running Task ");
                    char buf[16];
                    itoa(task2->id, buf, 10);
                    vga_print(buf);
                    vga_print(":\n");
                    
                    void (*entry_func)(void) = (void (*)(void))task2->context.eip;
                    entry_func();
                    
                    vga_print("\n[+] Task ");
                    itoa(task2->id, buf, 10);
                    vga_print(buf);
                    vga_print(" completed\n\n");
                }
                
                vga_print("[+] All tasks completed\n");
            } else if (strcmp(input, "iotest") == 0) {
                vga_print("\n[*] Running I/O Subsystem Tests (Day 10)...\n");
                task_io_full_test();
            } else if (strcmp(input, "fdinfo") == 0) {
                vga_print("\n[*] File Descriptor Table:\n");
                fd_table_t *table = fd_get_current_table();
                fd_print_table(table);
            } else if (strcmp(input, "pipetest") == 0) {
                vga_print("\n[*] Running Pipe Test...\n");
                task_io_pipe_demo();
            } else if (strcmp(input, "disktest") == 0) {
                vga_print("\n[*] Running Disk Read/Write Test (Day 11)...\n");

                if (!ata_is_present()) {
                    vga_print("[-] No ATA disk detected on primary master.\n");
                    vga_print("    If you're using QEMU, attach a disk (e.g. -hda disk.img).\n");
                    vga_print("    The current Makefile 'run' target boots with -kernel and no disk.\n");
                    goto disktest_done;
                }
                
                // Test data pattern
                uint8_t test_data[512];
                uint8_t read_data[512];
                
                // Fill with test pattern: 0xAA, 0x55, 0xAA, 0x55...
                for (int i = 0; i < 512; i++) {
                    test_data[i] = (i % 2 == 0) ? 0xAA : 0x55;
                }
                
                vga_print("[*] Writing test pattern to disk block 10...\n");
                int write_result = block_write(10, test_data);
                
                if (write_result == 0) {
                    vga_print("[+] Write successful\n");
                    
                    vga_print("[*] Reading back from disk block 10...\n");
                    int read_result = block_read(10, read_data);
                    
                    if (read_result == 0) {
                        vga_print("[+] Read successful\n");
                        
                        vga_print("[*] Verifying data integrity...\n");
                        int verified = 1;
                        int first_error = -1;
                        
                        for (int i = 0; i < 512; i++) {
                            if (read_data[i] != test_data[i]) {
                                verified = 0;
                                if (first_error == -1) {
                                    first_error = i;
                                }
                            }
                        }
                        
                        if (verified) {
                            vga_print("[+] Data verification PASSED - disk I/O working correctly!\n");
                            vga_print("[+] Block device abstraction (Day 11) is functional\n");
                        } else {
                            vga_print("[-] Data verification FAILED!\n");
                            vga_print("    First error at byte ");
                            char buf[16];
                            itoa(first_error, buf, 10);
                            vga_print(buf);
                            vga_print("\n");
                            vga_print("    Expected: 0x");
                            itoa(test_data[first_error], buf, 16);
                            vga_print(buf);
                            vga_print(", Got: 0x");
                            itoa(read_data[first_error], buf, 16);
                            vga_print(buf);
                            vga_print("\n");
                        }
                    } else {
                        vga_print("[-] Read failed with error code ");
                        char buf[16];
                        itoa(read_result, buf, 10);
                        vga_print(buf);
                        vga_print("\n");
                    }
                } else {
                    vga_print("[-] Write failed with error code ");
                    char buf[16];
                    itoa(write_result, buf, 10);
                    vga_print(buf);
                    vga_print("\n");
                }
                
                vga_print("[*] Flushing block cache...\n");
                block_flush();
                vga_print("[+] Disk test completed\n");

disktest_done:
;
            } else if (strcmp(input, "diskinfo") == 0) {
                vga_print("\n[*] Disk and Block Cache Information:\n");
                
                vga_print("Block Device Status:\n");
                vga_print("  Block size: 512 bytes\n");
                vga_print("  Cache size: ");
                char buf[16];
                itoa(BLOCK_CACHE_SIZE, buf, 10);
                vga_print(buf);
                vga_print(" entries (");
                itoa(BLOCK_CACHE_SIZE * 512 / 1024, buf, 10);
                vga_print(buf);
                vga_print(" KB)\n");
                
                vga_print("Cache Statistics:\n");
                int valid_entries = block_get_cache_valid_count();
                int dirty_entries = block_get_cache_dirty_count();
                
                vga_print("  Valid entries: ");
                itoa(valid_entries, buf, 10);
                vga_print(buf);
                vga_print("/");
                itoa(BLOCK_CACHE_SIZE, buf, 10);
                vga_print(buf);
                vga_print("\n");
                
                vga_print("  Dirty entries: ");
                itoa(dirty_entries, buf, 10);
                vga_print(buf);
                vga_print(" (need flushing)\n");
                
                vga_print("I/O Queue Status:\n");
                vga_print("  Queue size: ");
                itoa(IO_QUEUE_SIZE, buf, 10);
                vga_print(buf);
                vga_print(" slots\n");
                
                vga_print("  Pending requests: ");
                itoa(block_get_queue_pending_count(), buf, 10);
                vga_print(buf);
                vga_print("\n");
                
                // Test ATA identify if possible
                vga_print("ATA Drive Status:\n");
                if (!ata_is_present()) {
                    vga_print("  Drive detected: No\n");
                } else {
                    uint16_t identify_data[256];
                    int identify_result = ata_identify(identify_data);
                    if (identify_result == 0) {
                        vga_print("  Drive detected: Yes\n");
                    vga_print("  Serial Number: ");
                    // Serial number is at words 10-19 (20 bytes, little endian)
                    for (int i = 19; i >= 10; i--) {
                        char c1 = (identify_data[i] >> 8) & 0xFF;
                        char c2 = identify_data[i] & 0xFF;
                        if (c1 >= 32 && c1 <= 126) vga_putc(c1);
                        if (c2 >= 32 && c2 <= 126) vga_putc(c2);
                    }
                    vga_print("\n");
                    
                    vga_print("  Model Number: ");
                    // Model number is at words 27-46 (40 bytes, little endian)
                    for (int i = 46; i >= 27; i--) {
                        char c1 = (identify_data[i] >> 8) & 0xFF;
                        char c2 = identify_data[i] & 0xFF;
                        if (c1 >= 32 && c1 <= 126) vga_putc(c1);
                        if (c2 >= 32 && c2 <= 126) vga_putc(c2);
                    }
                    vga_print("\n");
                    } else {
                        vga_print("  Drive detected: No (or not responding)\n");
                    }
                }
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