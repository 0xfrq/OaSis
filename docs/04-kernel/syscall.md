---
layout: default
title: system Calls
---

# System Calls

## Calling Convention

all system calls use `int 0x80` with arguments in registers:

```text
eax = syscall number
ebx = arg1
ecx = arg2
edx = arg3
Return value in eax (negative = error)
```

### From Ring 0 (kernel/assembler)
```asm
mov eax, 9       ; SYSCALL_OPEN
mov ebx, path    ; pointer to path string
mov ecx, 0       ; O_RDONLY
int 0x80
; eax = fd or -1
```

### From Ring 3 (user mode)
same mechanism — the handler detects ring 3 by checking CS at [esp+36] and handles the larger iret frame (5 words vs 3).

## Syscall Dispatch

`int_80_handler(syscall_num, arg1, arg2, arg3)` delegates to `syscall_dispatch()`, which uses a switch statement to call the appropriate handler function.

## Complete system call table

| # | name | Args | Description | Impl |
|---|------|------|-------------|------|
| 0 | SYSCALL_WRITE | msg, len | Legacy write to stdout | `fd_write(STDOUT_FILENO)` |
| 1 | SYSCALL_SLEEP | ms | Sleep for milliseconds | Stub (returns 0) |
| 2 | SYSCALL_YIELD | — | Yield CPU to scheduler | `task_switch()` |
| 3 | SYSCALL_EXIT | exit_code | Exit current process | Marks task_DEAD, closes fds |
| 4 | SYSCALL_GETPID | — | Get process ID | Returns `current_task->id` |
| 5 | SYSCALL_FORK | — | Fork process | `task_fork()` |
| 6 | SYSCALL_EXEC | program, size | Execute program | `task_exec()` |
| 7 | SYSCALL_WAIT | status | Wait for child | `task_wait(status)` |
| 8 | SYSCALL_GETPPID | — | Get parent PID | Returns `current_task->ppid` |
| 9 | SYSCALL_open | path, flags | open file | `fd_open(table, path, flags)` |
| 10 | SYSCALL_close | fd | close fd | `fd_close(table, fd)` |
| 11 | SYSCALL_read | fd, buf, count | read from fd | `fd_read(table, fd, buf, count)` |
| 12 | SYSCALL_WRITE_FD | fd, buf, count | write to fd | `fd_write(table, fd, buf, count)` |
| 13 | SYSCALL_PIPE | pipefd[2] | Create pipe | `fd_pipe(table, pipefd)` |
| 14 | SYSCALL_DUP | oldfd | Duplicate fd | `fd_dup(table, oldfd)` |
| 15 | SYSCALL_DUP2 | oldfd, newfd | Dup2 fd | `fd_dup2(table, oldfd, newfd)` |
| 16 | SYSCALL_SEEK | fd, offset, whence | Seek in fd | `fd_seek(table, fd, offset, whence)` |
| 17 | SYSCALL_FDINFO | — | Show fd table | Prints kernel fd list |
| 18 | SYSCALL_block_read | block, buf | read disk block | `block_read(block, buf)` |
| 19 | SYSCALL_block_write | block, buf | write disk block | `block_write(block, buf)` |
| 20 | SYSCALL_block_FLUSH | — | Flush disk cache | `block_flush()` |
| 21 | SYSCALL_USER_EXIT | — | Exit user mode | Sets `user_exit_flag`, triggers iret redirect |
| 22 | SYSCALL_BRK | addr | User heap brk | Allocates pages at heap address |

## User mode exit flow

when `SYSCALL_USER_EXIT` is called from ring 3:

1. `syscall_user_exit()` in C sets `user_exit_flag = 1`.
2. Back in `int_80_wrapper` (assembly), the flag is checked.
3. if set, the iret frame is overwritten:
   - EIP = `user_exit_eip` (address of `user_return_to_shell`)
   - CS = `0x08`, ESP = `user_exit_esp`, SS = `0x10`
4. after IRET, execution jumps to `user_return_to_shell`:
   - Restores CR3 to `kernel_page_dir`
   - Restores stack via `user_exit_esp`
   - Restores EBP and returns to shell.

## External symbols

The built-in assembler maps these symbol names to kernel functions:

| Symbol | function | Description |
|--------|----------|-------------|
| `_printf` | `klibc_printf` | Direct VGA printf (ring 0) |
| `_putchar` | `klibc_putchar` | Direct VGA putchar |
| `_malloc` | `klibc_malloc` | Kernel heap alloc |
| `_free` | `klibc_free` | Kernel heap free |
| `_sys_open` | `syscall_open` | file open (both rings) |
| `_sys_read` | `syscall_read` | file read |
| `_sys_write_fd` | `syscall_write_fd` | file write |
| `_usr_printf` | `usr_printf` | Printf via syscall (ring 3 safe) |
