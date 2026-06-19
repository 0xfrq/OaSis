# System Calls

OaSis menyediakan 23 system calls via `int 0x80`. System call adalah cara program (ring 0 maupun ring 3) meminta service dari kernel.

## Mekanisme

1. Program mengisi register: `eax` = nomor syscall, `ebx`-`edx` = argumen
2. Eksekusi `int 0x80`
3. CPU switch ke kernel mode (ring 0)
4. Handler `int_80_wrapper` menyimpan register, mendeteksi ring asal (ring 0 vs ring 3)
5. `int_80_handler` memanggil `syscall_dispatch()`
6. Hasil dikembalikan via `eax`

## Syscall Table

| # | Nama | Argumen | Keterangan |
|---|------|---------|------------|
| 0 | SYSCALL_WRITE | msg, len | Tulis ke stdout (legacy) |
| 1 | SYSCALL_SLEEP | ms | Sleep (stub) |
| 2 | SYSCALL_YIELD | - | Yield task ke scheduler |
| 3 | SYSCALL_EXIT | exit_code | Exit process |
| 4 | SYSCALL_GETPID | - | Dapatkan PID |
| 5 | SYSCALL_FORK | - | Fork process |
| 6 | SYSCALL_EXEC | program, size | Exec program |
| 7 | SYSCALL_WAIT | status | Wait child process |
| 8 | SYSCALL_GETPPID | - | Dapatkan parent PID |
| 9 | SYSCALL_OPEN | path, flags | Buka file |
| 10 | SYSCALL_CLOSE | fd | Tutup file descriptor |
| 11 | SYSCALL_READ | fd, buf, count | Baca dari fd |
| 12 | SYSCALL_WRITE_FD | fd, buf, count | Tulis ke fd |
| 13 | SYSCALL_PIPE | pipefd[2] | Buat pipe |
| 14 | SYSCALL_DUP | oldfd | Duplicate fd |
| 15 | SYSCALL_DUP2 | oldfd, newfd | Dup ke fd spesifik |
| 16 | SYSCALL_SEEK | fd, offset, whence | Seek di fd |
| 17 | SYSCALL_FDINFO | - | Debug: print fd table |
| 18 | SYSCALL_BLOCK_READ | block, buf | Baca block device |
| 19 | SYSCALL_BLOCK_WRITE | block, buf | Tulis ke block device |
| 20 | SYSCALL_BLOCK_FLUSH | - | Flush block cache |
| 21 | SYSCALL_USER_EXIT | - | Exit user mode -> return to shell |
| 22 | SYSCALL_BRK | addr | Expand/shrink user heap |

## Dari Ring 3 (User Mode)

Program user memanggil syscall via `int 0x80`:
```asm
mov eax, 0        ; SYSCALL_WRITE
mov ebx, msg      ; pointer string
mov ecx, 5        ; panjang
int 0x80
```

Untuk exit dan balik ke shell:
```asm
mov eax, 21       ; SYSCALL_USER_EXIT
int 0x80
```

## Dari Ring 0 (Kernel/Assembler)

Syscall juga bisa dipanggil dari kernel mode via inline assembly.

## External Symbols (asm.c)

Built-in assembler menyediakan external symbol yang resolve ke fungsi kernel:

| Symbol | Fungsi | Deskripsi |
|--------|--------|-----------|
| `_printf` | `klibc_printf` | Printf via VGA |
| `_putchar` | `klibc_putchar` | Putchar via VGA |
| `_puts` | `klibc_puts` | Puts via VGA |
| `_malloc` | `klibc_malloc` | Alokasi heap kernel |
| `_free` | `klibc_free` | Free heap kernel |
| `_calloc` | `klibc_calloc` | Calloc kernel |
| `_realloc` | `klibc_realloc` | Realloc kernel |
| `_sys_open` | `syscall_open` | Open file |
| `_sys_read` | `syscall_read` | Read file |
| `_sys_write_fd` | `syscall_write_fd` | Write file |
| `_sys_close` | `syscall_close` | Close file |
| `_usr_printf` | `usr_printf` | Printf via syscall |
| `_usr_puts` | `usr_puts` | Puts via syscall |

Contoh dari assembly:
```asm
push 0
push path
call _sys_open
add esp, 8
; eax = fd
```
