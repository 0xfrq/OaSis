#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define TASK_MAX 16
#define TASK_STACK_SIZE 4096

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_DEAD
} task_state_t;

typedef struct {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp, esp;
    uint32_t eip;
    uint32_t eflags;
    uint32_t cs;
    uint32_t cr3;
} task_context_t;

typedef struct task_t {
    uint32_t id;
    task_state_t state;
    task_context_t context;
    uint32_t *stack;
    uint32_t stack_base;
    struct task_t *next;
    struct task_t *prev;
} task_t;

// Global current task pointer
extern task_t *current_task;

// Function declarations
void task_init(void);
task_t *task_create(void (*entry)(void));
void task_yield(void);
void task_switch(void);
task_t *task_get_current(void);
task_t *get_task_ptr(int id);
void task_print_info(void);

#endif // TASK_H