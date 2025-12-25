#ifndef TASKS_IO_H
#define TASKS_IO_H

/*
 * Day 10: I/O Demo Tasks
 * 
 * These tasks demonstrate the I/O subsystem capabilities:
 * - Standard I/O (stdin, stdout, stderr)
 * - Pipe communication between tasks
 * - File descriptor operations
 */

/* Demo: Write to stdout using fd-based syscalls */
void task_io_stdout_demo(void);

/* Demo: Read from stdin */
void task_io_stdin_demo(void);

/* Demo: Pipe communication */
void task_io_pipe_demo(void);

/* Demo: File descriptor duplication */
void task_io_dup_demo(void);

/* Demo: Print file descriptor table */
void task_io_fdinfo_demo(void);

/* Demo: Complete I/O test */
void task_io_full_test(void);

#endif /* TASKS_IO_H */
