#include "vga.h"
#include "gdt.h"
#include "keyboard.h"
#include "task_user.h"
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
#include "vfs.h"
#include "editor.h"
#include "asm.h"
#include "log.h"

#include "lexer.h"
#include "parser.h"
#include "codegen.h"
#include "asm.h"
#include "log.h"

// batas panjang input shell
#define INPUT_MAX 256

// Helper function to test lexer
void test_lexer(const char *path) {
    int fd = vfs_open(path, VFS_O_READ);
    if (fd < 0) {
        vga_print("lex: gagal buka file\n");
        return;
    }

    static char buf[4096];
    int n = vfs_read(fd, buf, sizeof(buf) - 1);
    vfs_close(fd);
    if (n <= 0) return;
    buf[n] = 0;

    lexer_t *l = lexer_create(buf, n);
    token_t tok;
    do {
        tok = lexer_next_token(l);
        char line_buf[16], col_buf[16];
        itoa(tok.line, line_buf, 10);
        itoa(tok.column, col_buf, 10);

        vga_print("[");
        vga_print(line_buf);
        vga_print(":");
        vga_print(col_buf);
        vga_print("] Type: ");

        char type_buf[16];
        itoa(tok.type, type_buf, 10);
        vga_print(type_buf);
        vga_print(" Val: '");
        vga_print(tok.value);
        vga_print("'\n");
    } while (tok.type != TOKEN_EOF && tok.type != TOKEN_ERROR);
}

// Helper function to test parser
void test_parser(const char *path) {
    int fd = vfs_open(path, VFS_O_READ);
    if (fd < 0) {
        vga_print("parse: gagal buka file\n");
        return;
    }

    static char buf[4096];
    int n = vfs_read(fd, buf, sizeof(buf) - 1);
    vfs_close(fd);
    if (n <= 0) return;
    buf[n] = 0;

    ast_pool_reset();

    parser_t p;
    p.lexer = lexer_create(buf, n);
    p.has_error = 0;
    p.current_token = lexer_next_token(p.lexer);
    p.peek_token = lexer_next_token(p.lexer);

    ast_node_t *program = parser_parse_program(&p);

    if (p.has_error) {
        vga_print("parse: syntax error detected\n");
    } else {
        vga_print("=== AST ===\n");
        ast_print(program, 0);
        vga_print("=== END ===\n");
    }
}

// Compile and run a C file using occ
void run_occ(const char *path) {
    vga_set_color(15, 0);

    int fd = vfs_open(path, VFS_O_READ);
    if (fd < 0) {
        vga_print("occ: gagal buka file '");
        vga_print(path);
        vga_print("'\n");
        return;
    }

    static char buf[32768];
    /* clear buffer to avoid leftover from previous call */
    for (int i = 0; i < 32768; i++) buf[i] = 0;
    int n = vfs_read(fd, buf, sizeof(buf) - 1);
    vfs_close(fd);
    if (n <= 0) {
        vga_print("occ: file kosong\n");
        return;
    }
    buf[n] = 0;

    /* Lex + Parse */
    ast_pool_reset();
    parser_t p;
    p.lexer = lexer_create(buf, n);
    p.has_error = 0;
    p.current_token = lexer_next_token(p.lexer);
    p.peek_token = lexer_next_token(p.lexer);

    ast_node_t *program = parser_parse_program(&p);
    if (p.has_error) {
        vga_print("occ: syntax error\n");
        return;
    }

    /* Codegen */
    codegen_t *cg = codegen_create();
    if (codegen_program(cg, program) < 0) {
        vga_print("occ: codegen gagal\n");
        return;
    }

    /* Write generated assembly to /tmp.s so nasm can read it */
    static char asm_path[] = "/tmp.s";
    int wfd = vfs_open(asm_path, VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC);
    if (wfd < 0) {
        vga_print("occ: gagal tulis /tmp.s\n");
        return;
    }
    const char *out = codegen_get_output(cg);
    int wn = vfs_write(wfd, out, (uint32_t)strlen(out));
    vfs_close(wfd);
    (void)wn;

    /* Assemble and run */
    vga_print("[occ] compiling...\n");
    asm volatile("sti"); /* pastikan interrupt nyala sebelum jalanin fn() */
    keyboard_flush();
    asm_run_file(asm_path);
    keyboard_flush();
    asm volatile("sti"); /* pastikan interrupt tetep nyala setelah fn() */
    vga_print("\n[occ] done.\n");
    vga_refresh_cursor();
}

// batas output listing filesystem
#define FS_OUT_MAX 2048

// cek string dimulai dengan prefix tertentu gak
static int starts_with(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s++ != *prefix++) return 0;
    }
    return 1;
}

// buang spasi di awal string
static void trim_leading_spaces(char **p) {
    while (**p == ' ') (*p)++;
}

// hitung panjang string manual
static uint32_t local_strlen(const char *s) {
    uint32_t n = 0;
    while (s && s[n]) n++;
    return n;
}

void kernel_main(void) {
    vga_clear();
    vga_cursor_init();
    vga_print("=== OASIS ===\n");

    vga_print("[*] Setting up GDT with user mode segments...\n");
    gdt_init();

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

        log_init();
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

    vga_print("\n[*] Initializing filesystem (OAFS)...\n");
    if (vfs_init() != 0) {
        vga_print("[-] Filesystem init failed\n");
    }

    vga_print("\n[*] Initializing system calls...\n");
    syscall_init();

    vga_print("[*] Creating tasks...\n");

    task_create(task_idle);
    task_create(task_worker);
    task_create(task_block_test);

    vga_print("[+] Tasks created and ready\n");
    vga_print("[*] Tasks managed by scheduler (timer-driven)\n");
    vga_print("Type 'help' for commands\n\n");

    char input[INPUT_MAX];
    int index = 0;
    
    {
        char cwd_buf[256];
        vfs_getcwd(cwd_buf, sizeof(cwd_buf));
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_print("oasis");
        vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
        vga_putc('(');
        vga_print(cwd_buf);
        vga_putc(')');
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        vga_print("> ");
        vga_set_color(15, VGA_COLOR_BLACK);
        vga_refresh_cursor();
    }

    while (1) {
        char c = keyboard_getchar();

        if(c=='\n') {
            input[index] = 0;
            vga_putc('\n');
            vga_refresh_cursor();

            if(strcmp(input, "help") == 0) {
                vga_print("Perintah:\n");
                vga_print("  help        - tampilkan pesan ini\n");
                vga_print("  help more   - tampilkan semua perintah\n");
                vga_print("  clear       - bersihkan layar\n");
                vga_print("  edit <p>    - editor teks (kayak nano)\n");
                vga_print("  cat <p>     - tampilkan isi file\n");
                vga_print("  write <p> <text> - tulis ke file\n");
                vga_print("  asm         - mode assembler interaktif\n");
                vga_print("  nasm <p>    - assemble & jalanin file .asm\n");
                vga_print("  lex <p>     - tokenisasi file C\n");
                vga_print("  parse <p>   - parse file C jadi AST\n");
                vga_print("  occ <p>     - compile & jalanin file .c (printf)\n");
            } else if (strcmp(input, "help more") == 0) {
                vga_print("Semua perintah:\n");
                vga_print("  help / help more - bantuan\n");
                vga_print("  clear            - bersihkan layar\n");
                vga_print("  edit <p>         - editor teks\n");
                vga_print("  cat <p>          - tampilkan isi file\n");
                vga_print("  write <p> <text> - tulis ke file\n");
                vga_print("  append <p> <text>- append ke file\n");
                vga_print("  ls [path]        - list direktori\n");
                vga_print("  pwd              - print working dir\n");
                vga_print("  cd <path>        - pindah direktori\n");
                vga_print("  mkdir <p>        - buat direktori\n");
                vga_print("  touch <p>        - buat file kosong\n");
                vga_print("  rm <p>           - hapus file\n");
                vga_print("  rmdir <p>        - hapus direktori\n");
                vga_print("  asm              - assembler interaktif\n");
                vga_print("  nasm <p>         - jalankan .asm\n");
                vga_print("  lex <p>          - tokenisasi C\n");
                vga_print("  parse <p>        - parse C ke AST\n");
                vga_print("  occ <p>          - compile C & run\n");
                vga_print("  uptime / meminfo - info sistem\n");
            } else if (strcmp(input, "asm") == 0) {
                asm_run();
            } else if (starts_with(input, "edit ")) {
                char *arg = input + 5;
                trim_leading_spaces(&arg);
                if (*arg == 0) {
                    vga_print("edit: cara pake: edit <nama file>\n");
                } else {
                    editor_run(arg);
                }
            } else if (starts_with(input, "nasm ")) {
                char *arg = input + 5;
                trim_leading_spaces(&arg);
                if (*arg == 0) {
                    vga_print("nasm: cara pake: nasm <file.asm>\n");
                } else {
                    asm_run_file(arg);
                }
            } else if (starts_with(input, "user ")) {
                char *arg = input + 5;
                trim_leading_spaces(&arg);
                if (*arg == 0) {
                    vga_print("user: cara pake: user <file.asm>\n");
                } else {
                    /* Baca file .asm, compile, dan jalanin di ring 3 */
                    int fd = vfs_open(arg, VFS_O_READ);
                    if (fd < 0) {
                        vga_print("user: file tidak ditemukan\n");
                    } else {
                        char buf[4096];
                        int n = vfs_read(fd, buf, sizeof(buf) - 1);
                        vfs_close(fd);
                        if (n > 0) {
                            buf[n] = 0;
                            run_user_test(buf);
                        }
                    }
                    /* User task returned, shell continues normally */
                }
            } else if (starts_with(input, "lex ")) {
                char *arg = input + 4;
                trim_leading_spaces(&arg);
                if (*arg == 0) {
                    vga_print("lex: cara pake: lex <file.c>\n");
                } else {
                    test_lexer(arg);
                }
            } else if (starts_with(input, "parse ")) {
                char *arg = input + 6;
                trim_leading_spaces(&arg);
                if (*arg == 0) {
                    vga_print("parse: cara pake: parse <file.c>\n");
                } else {
                    test_parser(arg);
                }
            } else if (starts_with(input, "occ ")) {
                char *arg = input + 4;
                trim_leading_spaces(&arg);
                if (*arg == 0) {
                    vga_print("occ: cara pake: occ <file.c>\n");
                } else {
                    run_occ(arg);
                }
            } else if (strcmp(input, "syscall") == 0) {
                vga_print("=== System Calls (int 0x80) ===\n");
                vga_print(" 0  SYSCALL_WRITE       - write ke stdout\n");
                vga_print(" 1  SYSCALL_SLEEP       - sleep (ms)\n");
                vga_print(" 2  SYSCALL_YIELD       - yield task\n");
                vga_print(" 3  SYSCALL_EXIT        - exit task\n");
                vga_print(" 4  SYSCALL_GETPID      - get PID\n");
                vga_print(" 5  SYSCALL_FORK        - fork process\n");
                vga_print(" 6  SYSCALL_EXEC        - exec program\n");
                vga_print(" 7  SYSCALL_WAIT        - wait child\n");
                vga_print(" 8  SYSCALL_GETPPID     - get parent PID\n");
                vga_print(" 9  SYSCALL_OPEN        - open file\n");
                vga_print("10  SYSCALL_CLOSE       - close fd\n");
                vga_print("11  SYSCALL_READ        - read fd\n");
                vga_print("12  SYSCALL_WRITE_FD    - write fd\n");
                vga_print("13  SYSCALL_PIPE        - pipe\n");
                vga_print("14  SYSCALL_DUP         - dup fd\n");
                vga_print("15  SYSCALL_DUP2        - dup2 fd\n");
                vga_print("16  SYSCALL_SEEK        - seek fd\n");
                vga_print("17  SYSCALL_FDINFO      - fd info\n");
                vga_print("18  SYSCALL_BLOCK_READ  - read block\n");
                vga_print("19  SYSCALL_BLOCK_WRITE - write block\n");
                vga_print("20  SYSCALL_BLOCK_FLUSH - flush block\n");
                vga_print("21  SYSCALL_USER_EXIT   - exit user mode\n");
                vga_print("22  SYSCALL_BRK         - user heap brk\n");
                vga_print("=== Usage: eax=num, ebx=arg1, ecx=arg2, edx=arg3 ===\n");
            } else if (strcmp(input, "clear") == 0) {
                vga_clear();
            } else if (strcmp(input, "dmesg") == 0) {
                log_dump();
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
            } else if (starts_with(input, "hexdump ")) {
                char *arg = input + 8;
                trim_leading_spaces(&arg);
                if (*arg == 0) { vga_print("hexdump: missing path\n"); }
                else {
                    int fd = vfs_open(arg, VFS_O_READ);
                    if (fd < 0) { vga_print("hexdump: open failed\n"); }
                    else {
                        char hbuf[16]; int hn = 0;
                        while ((hn = vfs_read(fd, hbuf, 16)) > 0) {
                            for (int hi = 0; hi < hn; hi++) {
                                char hxs[4];
                                itoa((unsigned char)hbuf[hi], hxs, 16);
                                if ((unsigned char)hbuf[hi] < 0x10) vga_putc('0');
                                vga_print(hxs);
                                vga_putc(' ');
                            }
                            vga_putc('\n');
                        }
                        vfs_close(fd);
                    }
                }
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
                
                // bikin data buat tes
                uint8_t test_data[512];
                uint8_t read_data[512];

                // isi pake pola 0xAA, 0x55, 0xAA, 0x55...
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

                // coba ATA identify kalo bisa
                vga_print("ATA Drive Status:\n");
                if (!ata_is_present()) {
                    vga_print("  Drive detected: No\n");
                } else {
                    uint16_t identify_data[256];
                    int identify_result = ata_identify(identify_data);
                    if (identify_result == 0) {
                        vga_print("  Drive detected: Yes\n");
                        vga_print("  Serial Number: ");
                        // serial number ada di word 10-19 (20 byte, little endian)
                        for (int i = 19; i >= 10; i--) {
                            char c1 = (identify_data[i] >> 8) & 0xFF;
                            char c2 = identify_data[i] & 0xFF;
                            if (c1 >= 32 && c1 <= 126) vga_putc(c1);
                            if (c2 >= 32 && c2 <= 126) vga_putc(c2);
                        }
                        vga_print("\n");

                        vga_print("  Model Number: ");
                        // model number ada di word 27-46 (40 byte, little endian)
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
            } else if (strcmp(input, "pwd") == 0) {
                char pathbuf[256];
                vfs_getcwd(pathbuf, sizeof(pathbuf));
                vga_print(pathbuf);
                vga_print("\n");
            } else if (starts_with(input, "ls")) {
                char out[FS_OUT_MAX];
                char *arg = input + 2;
                trim_leading_spaces(&arg);
                if (*arg == 0) arg = ".";
                if (vfs_list(arg, out, sizeof(out)) >= 0) {
                    int j = 0;
                    while (out[j] != 0) {
                        if (out[j] == 'd') {
                            vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
                            j += 2;
                            while (out[j] != 0 && out[j] != '\n') {
                                vga_putc(out[j++]);
                            }
                            if (out[j] == '\n') j++;
                            vga_set_color(15, VGA_COLOR_BLACK);
                            vga_print("  ");
                        } else if (out[j] == 'f') {
                            vga_set_color(15, VGA_COLOR_BLACK);
                            j += 2;
                            while (out[j] != 0 && out[j] != '\n') {
                                vga_putc(out[j++]);
                            }
                            if (out[j] == '\n') j++;
                            vga_set_color(15, VGA_COLOR_BLACK);
                            vga_print("  ");
                        } else {
                            vga_putc(out[j]);
                            j++;
                        }
                    }
                    vga_putc('\n');
                } else {
                    vga_print("ls: failed\n");
                }
            } else if (starts_with(input, "cd ")) {
                char *arg = input + 3;
                trim_leading_spaces(&arg);
                if (*arg == 0 || vfs_chdir(arg) != 0) {
                    vga_print("cd: failed\n");
                }
            } else if (starts_with(input, "mkdir ")) {
                char *arg = input + 6;
                trim_leading_spaces(&arg);
                if (*arg == 0 || vfs_mkdir(arg) != 0) {
                    vga_print("mkdir: failed\n");
                }
            } else if (starts_with(input, "touch ")) {
                char *arg = input + 6;
                trim_leading_spaces(&arg);
                if (*arg == 0 || vfs_create(arg) != 0) {
                    vga_print("touch: failed\n");
                }
            } else if (starts_with(input, "rm ")) {
                char *arg = input + 3;
                trim_leading_spaces(&arg);
                if (*arg == 0 || vfs_unlink(arg) != 0) {
                    vga_print("rm: failed\n");
                }
            } else if (starts_with(input, "rmdir ")) {
                char *arg = input + 6;
                trim_leading_spaces(&arg);
                if (*arg == 0 || vfs_rmdir(arg) != 0) {
                    vga_print("rmdir: failed\n");
                }
            } else if (starts_with(input, "echo ")) {
                char *arg = input + 5;
                trim_leading_spaces(&arg);
                if (*arg == 0) { vga_putc('\n'); }
                else { vga_print(arg); vga_putc('\n'); }
            } else if (starts_with(input, "cat ")) {
                char *arg = input + 4;
                trim_leading_spaces(&arg);
                if (*arg == 0) {
                    vga_print("cat: missing path\n");
                } else {
                    int fd = vfs_open(arg, VFS_O_READ);
                    if (fd < 0) {
                        vga_print("cat: open failed\n");
                    } else {
                        char rbuf[128];
                        int n = 0;
                        while ((n = vfs_read(fd, rbuf, sizeof(rbuf) - 1)) > 0) {
                            rbuf[n] = 0;
                            vga_print(rbuf);
                        }
                        vfs_close(fd);
                        vga_print("\n");
                    }
                }
            } else if (starts_with(input, "write ") || starts_with(input, "append ")) {
                int append_mode = starts_with(input, "append ");
                char *arg = input + (append_mode ? 7 : 6);
                trim_leading_spaces(&arg);

                char *space = arg;
                while (*space && *space != ' ') space++;
                if (*space == 0) {
                    vga_print("write/append: usage write <path> <text>\n");
                } else {
                    *space = 0;
                    char *text = space + 1;
                    trim_leading_spaces(&text);

                    vga_print("[write] path='");
                    vga_print(arg);
                    vga_print("' text='");
                    vga_print(text);
                    vga_print("'\n");

                    /* cek dulu filenya udah ada apa belom */
                    uint32_t check_ino = 0;
                    int exists = (vfs_resolve_path(arg, &check_ino) == 0);
                    vga_print("[write] file exists: ");
                    vga_print(exists ? "yes" : "no");
                    vga_print("\n");

                    int fd = vfs_open(arg, VFS_O_WRITE | VFS_O_CREATE | (append_mode ? VFS_O_APPEND : VFS_O_TRUNC));
                    if (fd < 0) {
                        vga_print("write/append: open failed (fd=");
                        char dbuf[16];
                        itoa(fd, dbuf, 10);
                        vga_print(dbuf);
                        vga_print(")\n");
                    } else {
                        vga_print("[write] opened fd=");
                        char dbuf[16];
                        itoa(fd, dbuf, 10);
                        vga_print(dbuf);
                        vga_print("\n");
                        int written = vfs_write(fd, text, local_strlen(text));
                        vfs_close(fd);
                        if (written < 0) {
                            vga_print("write/append: write failed\n");
                        } else {
                            vga_print("[write] wrote ");
                            itoa(written, dbuf, 10);
                            vga_print(dbuf);
                            vga_print(" bytes\n");
                        }
                    }
                }
            } else if (index != 0) {
                vga_print("Perintah tidak dikenal: '");
                vga_print(input);
                vga_print("'\n");
            }

            index = 0;
            {
                char cwd_buf[256];
                vfs_getcwd(cwd_buf, sizeof(cwd_buf));
                vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
                vga_print("oasis");
                vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
                vga_putc('(');
                vga_print(cwd_buf);
                vga_putc(')');
                vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
                vga_print("> ");
                vga_set_color(15, VGA_COLOR_BLACK);
                vga_refresh_cursor();
            }
            continue;
        }

        if(c=='\b') {
            if(index > 0) {
                index--;
                vga_putc('\b');
                vga_refresh_cursor();
            }
            continue;
        }
        if(index < INPUT_MAX -1) {
            input[index++] = c;
            vga_putc(c);
            vga_refresh_cursor();
        }
    }
}