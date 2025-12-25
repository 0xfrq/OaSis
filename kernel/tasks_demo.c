#include "tasks_demo.h"
#include "syscall.h"

void task_idle(void) {
    for(int i=0; i<3; i++) {
        const char *msg = "  [IDLE] Running iteration...\n";
        sys_write(msg, 26);
        char buf[2];
        buf[0] = '0' + i;
        buf[1] = 0;
        sys_write(buf, 1);
        sys_write("\n", 1);
        sys_yield();
    }

    const char *msg = "  [IDLE] Done\n";
    sys_write(msg, 14);
}

void task_worker(void) {
    for (int i = 0; i < 3; i++) {
        const char *msg = "    [WORKER] Doing work iteration ";
        sys_write(msg, 34);
        
        char buf[2];
        buf[0] = '0' + i;
        buf[1] = 0;
        sys_write(buf, 1);
        
        sys_write("\n", 1);
        sys_yield();
    }
    
    const char *msg = "    [WORKER] Finished\n";
    sys_write(msg, 22);
}
