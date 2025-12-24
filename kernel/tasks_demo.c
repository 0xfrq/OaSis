#include "tasks_demo.h"
#include "syscall.h"

void task_idle(void) {
    for(int i=0; i<5; i++) {
        const char *msg = "  [IDLE] Yielding...\n";
        sys_write(msg, 21);
        sys_yield;
    }

    const char *msg = "  [IDLE] Exiting\n";
    sys_write(msg, 16);
    sys_exit(0);
}

void task_worker(void) {
    for(int i=0; i<5; i++) {
        const char *msg = "  [WORKER] Working...\n";
        sys_write(msg, 24);
        sys_yield();
    }

    const char *msg = "  [WORKER] Done\n";
    sys_write(msg, 17);
    sys_exit(0);
}