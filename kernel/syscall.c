#include "syscall.h"
#include "vga.h"
#include "task.h"
#include "idt.h"
#include "string.h"
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

uint32_t syscall_write(const char *msg, uint32_t len) {
    if (msg == NULL) return 0;
    if (len == 0) return 0;
    
    for (uint32_t i = 0; i < len; i++) {
        vga_putc(msg[i]);
    }
    
    return len;
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

uint32_t syscall_dispatch(uint32_t syscall_num, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    (void)arg3;  
    
    switch (syscall_num) {
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
        
        case SYSCALL_FORK:
            return syscall_fork();

        case SYSCALL_EXEC:
            return syscall_exec((const char *)arg1, arg2);

        case SYSCALL_WAIT:
            return syscall_wait((int *)arg1);
        
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


