#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define TASK_MAX 32
#define TASK_STACK_SIZE 0x1000

typedef enum {
    TASK_READY = 0,
    TASK_RUNNING = 1,
    TASK_BLOCKED = 2,
    TASK_DEAD = 3
} task_state_t;

typedef struct {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
} cpu_context_t;


typedef struct task {
    uint32_t id;
    task_state_t state;
    cpu_context_t context;
    uint32_t *stack;
    uint32_t stack_base;
    struct task *next;
    struct task *prev;
} task_t;

void task_init(void);
task_t *task_create(void (*entry)(void));
void task_yield(void);
void task_switch(void);
task_t *task_get_current(void);
void task_print_info(void);

extern task_t *current_task;

#endif