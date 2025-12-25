#ifndef TASK_H
#define TASK_H

#include <stdint.h>

#define TASK_MAX 16
#define TASK_STACK_SIZE 4096

/* Forward declaration for fd_table_t */
typedef struct fd_table fd_table_t;

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
    uint32_t ppid;
    task_state_t state;
    int exit_code;
    task_context_t context;
    uint32_t *stack;
    uint32_t stack_base;
    struct task *parent;
    struct task *child_first;
    struct task *sibling_next;
    struct task_t *next;
    struct task_t *prev;
    fd_table_t *fd_table;       /* Day 10: Per-process file descriptor table */
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

int task_fork(void);
int task_exec(const char *program, uint32_t size);
int task_wait(int *status);
task_t *task_find_child(task_t *parent);
void task_exit(int code);

#endif // TASK_H