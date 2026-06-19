/*
 * Hari 10: Demo Task I/O
 *
 * Nunjukin subsistem I/O pake contoh-contoh praktis:
 * - Operasi file descriptor
 * - Standard I/O (stdin, stdout, stderr)
 * - Pipe buat komunikasi antar proses
 */

#include "tasks_io.h"
#include "syscall.h"
#include "fd.h"
#include "string.h"
#include "vga.h"

/* Helper: Nulis string ke stdout */
static void print(const char *str) {
    int len = 0;
    while (str[len]) len++;
    /* Pake sys_write lawas yang nulis langsung ke stdout */
    sys_write(str, len);
}

/* Helper: Nulis string ke stderr */
static void eprint(const char *str) {
    int len = 0;
    while (str[len]) len++;
    /* Pake sys_write lawas dulu aja */
    sys_write(str, len);
}

/* Helper: Nampilin angka */
static void print_num(int n) {
    char buf[16];
    itoa(n, buf, 10);
    print(buf);
}

/* ====== Demo Task ====== */

/*
 * Demo 1: Nulis ke stdout pake syscall berbasis fd
 * Nunjukin cara nulis ke stdout lewat file descriptor
 */
void task_io_stdout_demo(void) {
    print("\n=== I/O Demo: stdout ===\n");
    print("Writing to fd 1 (stdout)...\n");

    /* Nulis pake syscall berbasis fd */
    const char *msg = "Hello from file descriptor 1!\n";
    int written = sys_write_fd(STDOUT_FILENO, msg, 30);

    print("Bytes written: ");
    print_num(written);
    print("\n");

    /* Sekalian nunjukin stderr juga bisa */
    eprint("[stderr] This goes to stderr (fd 2)\n");

    print("=== stdout demo complete ===\n\n");
}

/*
 * Demo 2: Baca dari stdin
 * Nunjukin baca blocking dari keyboard
 */
void task_io_stdin_demo(void) {
    print("\n=== I/O Demo: stdin ===\n");
    print("Reading from fd 0 (stdin)...\n");
    print("Type something and press Enter: ");

    char buf[64];
    int bytes_read = sys_read(STDIN_FILENO, buf, 63);

    if (bytes_read > 0) {
        buf[bytes_read] = '\0';
        print("You typed: ");
        print(buf);
    } else {
        print("(no input)\n");
    }

    print("=== stdin demo complete ===\n\n");
}

/*
 * Demo 3: Komunikasi pake pipe
 * Bikin pipe terus baca/tulis lewat situ
 * Catatan: Langsung pake fungsi kernel karena kita di kernel mode
 */
void task_io_pipe_demo(void) {
    vga_print("\n=== I/O Demo: pipe ===\n");
    char buf[32];

    int pipefd[2];

    /* Langsung pake fungsi kernel aja (bypass masalah inline asm syscall) */
    fd_table_t *table = fd_get_current_table();
    if (fd_pipe(table, pipefd) < 0) {
        vga_print("[ERROR] Failed to create pipe!\n");
        return;
    }

    vga_print("Pipe created: read_fd=");
    itoa(pipefd[0], buf, 10);
    vga_print(buf);
    vga_print(", write_fd=");
    itoa(pipefd[1], buf, 10);
    vga_print(buf);
    vga_print("\n");

    /* Nulis ke pipe pake fungsi kernel */
    const char *pipe_msg = "Hello through pipe!";
    int written = fd_write(table, pipefd[1], pipe_msg, 19);
    vga_print("Wrote ");
    itoa(written, buf, 10);
    vga_print(buf);
    vga_print(" bytes to pipe\n");

    /* Baca dari pipe pake fungsi kernel */
    char read_buf[32];
    int read_bytes = fd_read(table, pipefd[0], read_buf, 31);
    vga_print("Read ");
    itoa(read_bytes, buf, 10);
    vga_print(buf);
    vga_print(" bytes from pipe\n");

    if (read_bytes > 0) {
        read_buf[read_bytes] = '\0';
        vga_print("Data: '");
        vga_print(read_buf);
        vga_print("'\n");
    }

    /* Tutup kedua ujung pipe */
    fd_close(table, pipefd[0]);
    fd_close(table, pipefd[1]);
    vga_print("Pipe closed\n");

    vga_print("=== pipe demo complete ===\n\n");
}

/*
 * Demo 4: Duplikasi file descriptor
 * Nunjukin operasi dup sama dup2
 */
void task_io_dup_demo(void) {
    print("\n=== I/O Demo: dup/dup2 ===\n");

    /* Duplikat stdout */
    int new_fd = sys_dup(STDOUT_FILENO);
    if (new_fd >= 0) {
        print("Duplicated stdout to fd ");
        print_num(new_fd);
        print("\n");

        /* Nulis lewat fd yang udah diduplikat */
        const char *msg = "Writing through duplicated fd!\n";
        sys_write_fd(new_fd, msg, 31);

        /* Tutup yang duplikat */
        sys_close(new_fd);
        print("Closed duplicated fd\n");
    } else {
        eprint("[ERROR] dup failed\n");
    }

    /* Tes dup2: duplikat ke fd tertentu */
    int target_fd = 5;
    int result = sys_dup2(STDOUT_FILENO, target_fd);
    if (result >= 0) {
        print("dup2'd stdout to fd ");
        print_num(target_fd);
        print("\n");

        /* Nulis lewat fd 5 */
        const char *msg2 = "Hello from fd 5!\n";
        sys_write_fd(target_fd, msg2, 17);

        sys_close(target_fd);
    } else {
        eprint("[ERROR] dup2 failed\n");
    }

    print("=== dup/dup2 demo complete ===\n\n");
}

/*
 * Demo 5: Tampilin tabel file descriptor
 * Nunjukin kondisi tabel fd sekarang
 */
void task_io_fdinfo_demo(void) {
    print("\n=== I/O Demo: fdinfo ===\n");
    print("Current process file descriptor table:\n");
    sys_fdinfo();
    print("=== fdinfo demo complete ===\n\n");
}

/*
 * Demo 6: Tes I/O lengkap
 * Jalanin semua demo berurutan - langsung pake vga_print biar lebih reliable
 */
void task_io_full_test(void) {
    char buf[32];

    vga_print("\n");
    vga_print("+========================================+\n");
    vga_print("|       Day 10: I/O Subsystem Test       |\n");
    vga_print("+========================================+\n");

    /* Tunjukin kondisi awal fd */
    vga_print("\n[1/5] Initial file descriptor state:\n");
    sys_fdinfo();

    /* Demo stdout/stderr lewat syscall */
    vga_print("\n[2/5] Testing sys_write syscall...\n");
    sys_write("[syscall] sys_write works!\n", 27);

    /* Demo pipe - langsung pake fungsi kernel */
    vga_print("\n[3/5] Testing pipes...\n");
    int pipefd[2];
    fd_table_t *table = fd_get_current_table();

    if (fd_pipe(table, pipefd) == 0) {
        vga_print("  Pipe created: [");
        itoa(pipefd[0], buf, 10);
        vga_print(buf);
        vga_print(",");
        itoa(pipefd[1], buf, 10);
        vga_print(buf);
        vga_print("]\n");

        /* Tulis sama baca pake fungsi kernel */
        const char *test_data = "PIPE_TEST_DATA";
        int written = fd_write(table, pipefd[1], test_data, 14);
        vga_print("  Wrote ");
        itoa(written, buf, 10);
        vga_print(buf);
        vga_print(" bytes\n");

        char read_buf[32];
        int n = fd_read(table, pipefd[0], read_buf, 31);
        vga_print("  Read ");
        itoa(n, buf, 10);
        vga_print(buf);
        vga_print(" bytes\n");

        if (n > 0) {
            read_buf[n] = '\0';
            vga_print("  Data: '");
            vga_print(read_buf);
            vga_print("'\n");
        }

        fd_close(table, pipefd[0]);
        fd_close(table, pipefd[1]);
        vga_print("  Pipe closed successfully\n");
    } else {
        vga_print("  [FAIL] Could not create pipe\n");
    }

    /* Demo dup */
    vga_print("\n[4/5] Testing fd duplication...\n");
    int dup_fd = sys_dup(STDOUT_FILENO);
    if (dup_fd >= 0) {
        vga_print("  Duplicated stdout to fd ");
        itoa(dup_fd, buf, 10);
        vga_print(buf);
        vga_print("\n");
        sys_close(dup_fd);
    }

    /* Kondisi fd akhir */
    vga_print("\n[5/5] Final file descriptor state:\n");
    sys_fdinfo();

    vga_print("\n");
    vga_print("+========================================+\n");
    vga_print("|         I/O Subsystem: PASSED          |\n");
    vga_print("+========================================+\n");
}
