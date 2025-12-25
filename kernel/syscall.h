#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

#define SYSCALL_WRITE   0
#define SYSCALL_SLEEP   1
#define SYSCALL_YIELD   2
#define SYSCALL_EXIT    3
#define SYSCALL_GETPID  4
#define SYSCALL_FORK     5
#define SYSCALL_EXEC    6
#define SYSCALL_WAIT    7
#define SYSCALL_MAX     8

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

#endif
