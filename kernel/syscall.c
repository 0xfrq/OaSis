#include "syscall.h"
#include "vga.h"
#include "task.h"
#include "idt.h"
#include "string.h"
#include "fd.h"
#include <stddef.h>

extern uint32_t syscall_write(const char *msg, uint32_t len);
extern uint32_t syscall_sleep(uint32_t milliseconds);
extern uint32_t syscall_yield(void);
extern uint32_t syscall_exit(uint32_t exit_code);
extern uint32_t syscall_getpid(void);

typedef uint32_t (*syscall_handler_t)(void);

struct {
    uint32_t eax, ebx, ecx, edx;
} syscall_ctx;

static uint32_t syscall_invalid(void) {
    return 0xFFFFFFFF; 
}

/* Legacy write - writes directly to stdout */
uint32_t syscall_write(const char *msg, uint32_t len) {
    if (msg == NULL) return 0;
    if (len == 0) return 0;
    
    /* Use fd_write to stdout for consistency */
    fd_table_t *table = fd_get_current_table();
    return fd_write(table, STDOUT_FILENO, msg, len);
}

uint32_t syscall_sleep(uint32_t milliseconds) {
    (void)milliseconds;
    return 0;
}

uint32_t syscall_yield(void) {
    task_switch();
    return 0;
}

uint32_t syscall_exit(uint32_t exit_code) {
    task_t *current = task_get_current();
    if (current) {
        /* Close all file descriptors */
        if (current->fd_table) {
            fd_table_close_all(current->fd_table);
        }
        current->state = TASK_DEAD;
    }
    
    (void)exit_code;
    
    return 0;
}

uint32_t syscall_getpid(void) {
    task_t *current = task_get_current();
    if (current) {
        return current->id;
    }
    return 0;
}

uint32_t syscall_getppid(void) {
    task_t *current = task_get_current();
    if (current) {
        return current->ppid;
    }
    return 0;
}

uint32_t syscall_fork(void) {
    task_t *child = task_fork();
    if (child == NULL)
    {
        return 0xFFFFFFFF;
    }
    return child->id;
}


uint32_t syscall_exec(const char *program, uint32_t size) {
    task_exec(program, size);
    return 0;
}

uint32_t syscall_wait(int *status) {
    return task_wait(status);
}

/* ====== Day 10: I/O System Call Handlers ====== */

uint32_t syscall_open(const char *path, int flags) {
    fd_table_t *table = fd_get_current_table();
    return fd_open(table, path, flags);
}

uint32_t syscall_close(int fd) {
    fd_table_t *table = fd_get_current_table();
    return fd_close(table, fd);
}

uint32_t syscall_read(int fd, void *buf, uint32_t count) {
    fd_table_t *table = fd_get_current_table();
    return fd_read(table, fd, buf, count);
}

uint32_t syscall_write_fd(int fd, const void *buf, uint32_t count) {
    fd_table_t *table = fd_get_current_table();
    return fd_write(table, fd, buf, count);
}

uint32_t syscall_pipe(int pipefd[2]) {
    fd_table_t *table = fd_get_current_table();
    return fd_pipe(table, pipefd);
}

uint32_t syscall_dup(int oldfd) {
    fd_table_t *table = fd_get_current_table();
    return fd_dup(table, oldfd);
}

uint32_t syscall_dup2(int oldfd, int newfd) {
    fd_table_t *table = fd_get_current_table();
    return fd_dup2(table, oldfd, newfd);
}

uint32_t syscall_seek(int fd, int32_t offset, int whence) {
    fd_table_t *table = fd_get_current_table();
    return fd_seek(table, fd, offset, whence);
}

uint32_t syscall_fdinfo(void) {
    fd_table_t *table = fd_get_current_table();
    fd_print_table(table);
    return 0;
}

uint32_t syscall_dispatch(uint32_t syscall_num, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    switch (syscall_num) {
        /* Day 8: Basic syscalls */
        case SYSCALL_WRITE:
            return syscall_write((const char *)arg1, arg2);
        
        case SYSCALL_SLEEP:
            return syscall_sleep(arg1);
        
        case SYSCALL_YIELD:
            return syscall_yield();
        
        case SYSCALL_EXIT:
            return syscall_exit(arg1);
        
        case SYSCALL_GETPID:
            return syscall_getpid();
        
        /* Day 9: Process management */
        case SYSCALL_FORK:
            return syscall_fork();

        case SYSCALL_EXEC:
            return syscall_exec((const char *)arg1, arg2);

        case SYSCALL_WAIT:
            return syscall_wait((int *)arg1);
        
        case SYSCALL_GETPPID:
            return syscall_getppid();
        
        /* Day 10: I/O Subsystem */
        case SYSCALL_OPEN:
            return syscall_open((const char *)arg1, (int)arg2);
        
        case SYSCALL_CLOSE:
            return syscall_close((int)arg1);
        
        case SYSCALL_READ:
            return syscall_read((int)arg1, (void *)arg2, arg3);
        
        case SYSCALL_WRITE_FD:
            return syscall_write_fd((int)arg1, (const void *)arg2, arg3);
        
        case SYSCALL_PIPE:
            return syscall_pipe((int *)arg1);
        
        case SYSCALL_DUP:
            return syscall_dup((int)arg1);
        
        case SYSCALL_DUP2:
            return syscall_dup2((int)arg1, (int)arg2);
        
        case SYSCALL_SEEK:
            return syscall_seek((int)arg1, (int32_t)arg2, (int)arg3);
        
        case SYSCALL_FDINFO:
            return syscall_fdinfo();
        
        default:
            return syscall_invalid();
    }
}

uint32_t int_80_handler(uint32_t syscall_num, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    return syscall_dispatch(syscall_num, arg1, arg2, arg3);
}
extern void int_80_wrapper(void);

void syscall_init(void) {
    vga_print("[*] Initializing system call interface...\n");
    
    idt_set_entry(0x80, (uint32_t)&int_80_wrapper, 0x08, 0x8F);
    
    vga_print("[+] System call interface ready (INT 0x80)\n");
}


