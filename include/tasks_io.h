#ifndef TASKS_IO_H
#define TASKS_IO_H

/*
 * Day 10: Task Demo I/O
 *
 * Task-task ini nge-demo-in kemampuan subsystem I/O:
 * - I/O standar (stdin, stdout, stderr)
 * - Komunikasi pipe antar task
 * - Operasi file descriptor
 */

/* Demo: Tulis ke stdout pake syscall berbasis fd */
void task_io_stdout_demo(void);

/* Demo: Baca dari stdin */
void task_io_stdin_demo(void);

/* Demo: Komunikasi pake pipe */
void task_io_pipe_demo(void);

/* Demo: Duplikat file descriptor */
void task_io_dup_demo(void);

/* Demo: Print tabel file descriptor */
void task_io_fdinfo_demo(void);

/* Demo: Tes I/O lengkap */
void task_io_full_test(void);

#endif /* TASKS_IO_H */
