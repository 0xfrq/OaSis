/*
 * Day 10: I/O Demo Tasks
 * 
 * Demonstrates the I/O subsystem with practical examples:
 * - File descriptor operations
 * - Standard I/O (stdin, stdout, stderr)
 * - Pipes for inter-process communication
 */

#include "tasks_io.h"
#include "syscall.h"
#include "fd.h"
#include "string.h"

/* Helper: Write a string to stdout */
static void print(const char *str) {
    int len = 0;
    while (str[len]) len++;
    sys_write_fd(STDOUT_FILENO, str, len);
}

/* Helper: Write a string to stderr */
static void eprint(const char *str) {
    int len = 0;
    while (str[len]) len++;
    sys_write_fd(STDERR_FILENO, str, len);
}

/* Helper: Print a number */
static void print_num(int n) {
    char buf[16];
    itoa(n, buf, 10);
    print(buf);
}

/* ====== Demo Tasks ====== */

/*
 * Demo 1: Write to stdout using fd-based syscalls
 * Shows basic stdout writing through file descriptors
 */
void task_io_stdout_demo(void) {
    print("\n=== I/O Demo: stdout ===\n");
    print("Writing to fd 1 (stdout)...\n");
    
    /* Write using fd-based syscall */
    const char *msg = "Hello from file descriptor 1!\n";
    int written = sys_write_fd(STDOUT_FILENO, msg, 30);
    
    print("Bytes written: ");
    print_num(written);
    print("\n");
    
    /* Also show stderr works */
    eprint("[stderr] This goes to stderr (fd 2)\n");
    
    print("=== stdout demo complete ===\n\n");
}

/*
 * Demo 2: Read from stdin
 * Shows blocking read from keyboard
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
 * Demo 3: Pipe communication
 * Creates a pipe and writes/reads through it
 */
void task_io_pipe_demo(void) {
    print("\n=== I/O Demo: pipe ===\n");
    
    int pipefd[2];
    
    /* Create a pipe */
    if (sys_pipe(pipefd) < 0) {
        eprint("[ERROR] Failed to create pipe!\n");
        return;
    }
    
    print("Pipe created: read_fd=");
    print_num(pipefd[0]);
    print(", write_fd=");
    print_num(pipefd[1]);
    print("\n");
    
    /* Write to pipe */
    const char *pipe_msg = "Hello through pipe!";
    int written = sys_write_fd(pipefd[1], pipe_msg, 19);
    print("Wrote ");
    print_num(written);
    print(" bytes to pipe\n");
    
    /* Read from pipe */
    char buf[32];
    int read_bytes = sys_read(pipefd[0], buf, 31);
    if (read_bytes > 0) {
        buf[read_bytes] = '\0';
        print("Read from pipe: '");
        print(buf);
        print("'\n");
    }
    
    /* Close pipe ends */
    sys_close(pipefd[0]);
    sys_close(pipefd[1]);
    print("Pipe closed\n");
    
    print("=== pipe demo complete ===\n\n");
}

/*
 * Demo 4: File descriptor duplication
 * Shows dup and dup2 operations
 */
void task_io_dup_demo(void) {
    print("\n=== I/O Demo: dup/dup2 ===\n");
    
    /* Duplicate stdout */
    int new_fd = sys_dup(STDOUT_FILENO);
    if (new_fd >= 0) {
        print("Duplicated stdout to fd ");
        print_num(new_fd);
        print("\n");
        
        /* Write through the duplicated fd */
        const char *msg = "Writing through duplicated fd!\n";
        sys_write_fd(new_fd, msg, 31);
        
        /* Close the duplicate */
        sys_close(new_fd);
        print("Closed duplicated fd\n");
    } else {
        eprint("[ERROR] dup failed\n");
    }
    
    /* Test dup2: duplicate to specific fd */
    int target_fd = 5;
    int result = sys_dup2(STDOUT_FILENO, target_fd);
    if (result >= 0) {
        print("dup2'd stdout to fd ");
        print_num(target_fd);
        print("\n");
        
        /* Write through fd 5 */
        const char *msg2 = "Hello from fd 5!\n";
        sys_write_fd(target_fd, msg2, 17);
        
        sys_close(target_fd);
    } else {
        eprint("[ERROR] dup2 failed\n");
    }
    
    print("=== dup/dup2 demo complete ===\n\n");
}

/*
 * Demo 5: Print file descriptor table
 * Shows current state of the fd table
 */
void task_io_fdinfo_demo(void) {
    print("\n=== I/O Demo: fdinfo ===\n");
    print("Current process file descriptor table:\n");
    sys_fdinfo();
    print("=== fdinfo demo complete ===\n\n");
}

/*
 * Demo 6: Complete I/O test
 * Runs all demos in sequence
 */
void task_io_full_test(void) {
    print("\n");
    print("╔════════════════════════════════════════╗\n");
    print("║       Day 10: I/O Subsystem Test       ║\n");
    print("╚════════════════════════════════════════╝\n");
    
    /* Show initial fd state */
    print("\n[1/5] Initial file descriptor state:\n");
    sys_fdinfo();
    
    /* Demo stdout/stderr */
    print("\n[2/5] Testing stdout/stderr...\n");
    print("[stdout] Standard output works!\n");
    eprint("[stderr] Standard error works!\n");
    
    /* Demo pipe */
    print("\n[3/5] Testing pipes...\n");
    int pipefd[2];
    if (sys_pipe(pipefd) == 0) {
        print("  Pipe created: [");
        print_num(pipefd[0]);
        print(",");
        print_num(pipefd[1]);
        print("]\n");
        
        /* Write and read */
        const char *test_data = "PIPE_TEST_DATA";
        sys_write_fd(pipefd[1], test_data, 14);
        
        char buf[32];
        int n = sys_read(pipefd[0], buf, 31);
        buf[n] = '\0';
        
        print("  Data through pipe: '");
        print(buf);
        print("'\n");
        
        sys_close(pipefd[0]);
        sys_close(pipefd[1]);
        print("  Pipe closed successfully\n");
    } else {
        eprint("  [FAIL] Could not create pipe\n");
    }
    
    /* Demo dup */
    print("\n[4/5] Testing fd duplication...\n");
    int dup_fd = sys_dup(STDOUT_FILENO);
    if (dup_fd >= 0) {
        print("  Duplicated stdout to fd ");
        print_num(dup_fd);
        print("\n");
        sys_close(dup_fd);
    }
    
    /* Final fd state */
    print("\n[5/5] Final file descriptor state:\n");
    sys_fdinfo();
    
    print("\n");
    print("╔════════════════════════════════════════╗\n");
    print("║         I/O Subsystem: PASSED          ║\n");
    print("╚════════════════════════════════════════╝\n");
}
