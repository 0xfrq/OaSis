#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

/* Day 8: Basic syscalls */
#define SYSCALL_WRITE   0
#define SYSCALL_SLEEP   1
#define SYSCALL_YIELD   2
#define SYSCALL_EXIT    3
#define SYSCALL_GETPID  4

/* Day 9: Process management */
#define SYSCALL_FORK    5
#define SYSCALL_EXEC    6
#define SYSCALL_WAIT    7
#define SYSCALL_GETPPID 8

/* Day 10: I/O Subsystem */
#define SYSCALL_OPEN    9
#define SYSCALL_CLOSE   10
#define SYSCALL_READ    11
#define SYSCALL_WRITE_FD 12  /* Write with fd parameter */
#define SYSCALL_PIPE    13
#define SYSCALL_DUP     14
#define SYSCALL_DUP2    15
#define SYSCALL_SEEK    16
#define SYSCALL_FDINFO  17   /* Debug: print fd table */

/* Day 11: Block Device Abstraction */
#define SYSCALL_BLOCK_READ   18
#define SYSCALL_BLOCK_WRITE  19
#define SYSCALL_BLOCK_FLUSH  20

#define SYSCALL_MAX     21

void syscall_init(void);

uint32_t syscall_write(const char *msg, uint32_t len);
uint32_t syscall_sleep(uint32_t milliseconds);
uint32_t syscall_yield(void);
uint32_t syscall_exit(uint32_t exit_code);
uint32_t syscall_getpid(void);

static inline void sys_write(const char *msg, int len) {
    register uint32_t eax asm("eax") = SYSCALL_WRITE;
    register uint32_t ebx asm("ebx") = (uint32_t)msg;
    register uint32_t ecx asm("ecx") = len;
    
    asm volatile(
        "int $0x80"
        : "+r"(eax)
        : "r"(ebx), "r"(ecx)
        : "memory"
    );
}

static inline void sys_sleep(uint32_t ms) {
    register uint32_t eax asm("eax") = SYSCALL_SLEEP;
    register uint32_t ebx asm("ebx") = ms;
    
    asm volatile(
        "int $0x80"
        : "+r"(eax)
        : "r"(ebx)
    );
}

static inline void sys_yield(void) {
    register uint32_t eax asm("eax") = SYSCALL_YIELD;
    
    asm volatile(
        "int $0x80"
        : "+r"(eax)
    );
}

static inline void sys_exit(uint32_t code) {
    register uint32_t eax asm("eax") = SYSCALL_EXIT;
    register uint32_t ebx asm("ebx") = code;
    
    asm volatile(
        "int $0x80"
        : "+r"(eax)
        : "r"(ebx)
    );
    
    while(1);
}

static inline uint32_t sys_getpid(void) {
    register uint32_t eax asm("eax") = SYSCALL_GETPID;
    
    asm volatile(
        "int $0x80"
        : "+r"(eax)
    );
    
    return eax;
}

static inline int sys_fork(void) {
    register uint32_t eax asm("eax") = SYSCALL_FORK;

    asm volatile("int $0x80" : "+r"(eax));

    return (int)eax;
}

static inline int sys_exec(const char *program_ptr, uint32_t size) {
    register uint32_t eax asm("eax") = SYSCALL_EXEC;
    register uint32_t ebx asm("ebx") = (uint32_t)program_ptr;
    register uint32_t ecx asm("ecx") = size;

    asm volatile("int $0x80" : "+r"(eax) : "r"(ebx), "r"(ecx));
    
    return (int)eax;
}

static inline int sys_wait(int *status) {
    register uint32_t eax asm("eax") = SYSCALL_WAIT;
    register uint32_t ebx asm("ebx") = (uint32_t)status;

    asm volatile("int $0x80" : "+r"(eax) : "r"(ebx));

    return (int)eax;
}

/* Get parent PID */
static inline int sys_getppid(void) {
    register uint32_t eax asm("eax") = SYSCALL_GETPPID;
    
    asm volatile("int $0x80" : "+r"(eax));
    
    return (int)eax;
}

/* ====== Day 10: I/O System Calls ====== */

/* Open a file/device */
static inline int sys_open(const char *path, int flags) {
    register uint32_t eax asm("eax") = SYSCALL_OPEN;
    register uint32_t ebx asm("ebx") = (uint32_t)path;
    register uint32_t ecx asm("ecx") = (uint32_t)flags;
    
    asm volatile("int $0x80" : "+r"(eax) : "r"(ebx), "r"(ecx) : "memory");
    
    return (int)eax;
}

/* Close a file descriptor */
static inline int sys_close(int fd) {
    register uint32_t eax asm("eax") = SYSCALL_CLOSE;
    register uint32_t ebx asm("ebx") = (uint32_t)fd;
    
    asm volatile("int $0x80" : "+r"(eax) : "r"(ebx));
    
    return (int)eax;
}

/* Read from a file descriptor */
static inline int sys_read(int fd, void *buf, uint32_t count) {
    register uint32_t eax asm("eax") = SYSCALL_READ;
    register uint32_t ebx asm("ebx") = (uint32_t)fd;
    register uint32_t ecx asm("ecx") = (uint32_t)buf;
    register uint32_t edx asm("edx") = count;
    
    asm volatile("int $0x80" : "+r"(eax) : "r"(ebx), "r"(ecx), "r"(edx) : "memory");
    
    return (int)eax;
}

/* Write to a file descriptor (new fd-based write) */
static inline int sys_write_fd(int fd, const void *buf, uint32_t count) {
    register uint32_t eax asm("eax") = SYSCALL_WRITE_FD;
    register uint32_t ebx asm("ebx") = (uint32_t)fd;
    register uint32_t ecx asm("ecx") = (uint32_t)buf;
    register uint32_t edx asm("edx") = count;
    
    asm volatile("int $0x80" : "+r"(eax) : "r"(ebx), "r"(ecx), "r"(edx) : "memory");
    
    return (int)eax;
}

/* Create a pipe */
static inline int sys_pipe(int pipefd[2]) {
    register uint32_t eax asm("eax") = SYSCALL_PIPE;
    register uint32_t ebx asm("ebx") = (uint32_t)pipefd;
    
    asm volatile("int $0x80" : "+r"(eax) : "r"(ebx) : "memory");
    
    return (int)eax;
}

/* Duplicate a file descriptor */
static inline int sys_dup(int oldfd) {
    register uint32_t eax asm("eax") = SYSCALL_DUP;
    register uint32_t ebx asm("ebx") = (uint32_t)oldfd;
    
    asm volatile("int $0x80" : "+r"(eax) : "r"(ebx));
    
    return (int)eax;
}

/* Duplicate a file descriptor to specific fd */
static inline int sys_dup2(int oldfd, int newfd) {
    register uint32_t eax asm("eax") = SYSCALL_DUP2;
    register uint32_t ebx asm("ebx") = (uint32_t)oldfd;
    register uint32_t ecx asm("ecx") = (uint32_t)newfd;
    
    asm volatile("int $0x80" : "+r"(eax) : "r"(ebx), "r"(ecx));
    
    return (int)eax;
}

/* Seek within a file descriptor */
static inline int sys_seek(int fd, int32_t offset, int whence) {
    register uint32_t eax asm("eax") = SYSCALL_SEEK;
    register uint32_t ebx asm("ebx") = (uint32_t)fd;
    register uint32_t ecx asm("ecx") = (uint32_t)offset;
    register uint32_t edx asm("edx") = (uint32_t)whence;
    
    asm volatile("int $0x80" : "+r"(eax) : "r"(ebx), "r"(ecx), "r"(edx));
    
    return (int)eax;
}

/* Debug: print fd table */
static inline void sys_fdinfo(void) {
    register uint32_t eax asm("eax") = SYSCALL_FDINFO;
    
    asm volatile("int $0x80" : "+r"(eax));
}

/* Day 11: Block Device Abstraction */
static inline int sys_block_read(uint32_t block_num, void *buffer) {
    register uint32_t eax asm("eax") = SYSCALL_BLOCK_READ;
    register uint32_t ebx asm("ebx") = block_num;
    register uint32_t ecx asm("ecx") = (uint32_t)buffer;
    
    asm volatile("int $0x80" : "+r"(eax) : "r"(ebx), "r"(ecx));
    
    return (int)eax;
}

static inline int sys_block_write(uint32_t block_num, const void *buffer) {
    register uint32_t eax asm("eax") = SYSCALL_BLOCK_WRITE;
    register uint32_t ebx asm("ebx") = block_num;
    register uint32_t ecx asm("ecx") = (uint32_t)buffer;
    
    asm volatile("int $0x80" : "+r"(eax) : "r"(ebx), "r"(ecx));
    
    return (int)eax;
}

static inline void sys_block_flush(void) {
    register uint32_t eax asm("eax") = SYSCALL_BLOCK_FLUSH;
    
    asm volatile("int $0x80" : "+r"(eax));
}

#endif
