#include "tasks_demo.h"
#include "syscall.h"
#include <stddef.h>
// Day 10 task: Parent that forks a child
void task_parent(void) {
    const char *msg = "[PARENT] Starting\n";
    sys_write(msg, 19);
    
    // Fork a child process
    int child_pid = sys_fork();
    
    if (child_pid > 0) {
        // Parent process
        const char *parent_msg = "[PARENT] Forked child PID=";
        sys_write(parent_msg, 26);
        
        char buf[16];
        itoa(child_pid, buf, 10);
        sys_write(buf, 1);
        sys_write("\n", 1);
        
        // Wait for child to complete
        const char *wait_msg = "[PARENT] Waiting for child...\n";
        sys_write(wait_msg, 30);
        
        int status = 0;
        sys_wait(&status);
        
        const char *done_msg = "[PARENT] Child completed\n";
        sys_write(done_msg, 25);
    } else {
        // Child process
        const char *child_msg = "[CHILD] Starting, parent PID=";
        sys_write(child_msg, 29);
        
        int ppid = sys_getppid();
        char buf[16];
        itoa(ppid, buf, 10);
        sys_write(buf, 1);
        sys_write("\n", 1);
        
        const char *child_work = "[CHILD] Doing work...\n";
        sys_write(child_work, 22);
        
        sys_yield();
        
        const char *child_exit = "[CHILD] Exiting\n";
        sys_write(child_exit, 16);
        
        sys_exit(0);
    }
}

// Simpler parent task
void task_simple_parent(void) {
    const char *msg = "[SIMPLE] Hello from parent\n";
    sys_write(msg, 27);
    
    int child = sys_fork();
    if (child > 0) {
        sys_write("[SIMPLE] I am parent\n", 21);
        sys_wait(NULL);
        sys_write("[SIMPLE] Child done\n", 20);
    } else {
        sys_write("[SIMPLE] I am child\n", 20);
        sys_exit(0);
    }
}
