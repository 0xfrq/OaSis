#ifndef LIBC_SYSCALL_H
#define LIBC_SYSCALL_H

#include <stdint.h>

/* Nomor syscall OaSis (sesuai include/syscall.h kernel) */
#define SYS_WRITE 0
#define SYS_READ  11
#define SYS_EXIT  3

/* Wrapper internal syscall menggunakan int 0x80
 * eax = nomor syscall
 * ebx = arg1
 * ecx = arg2
 * edx = arg3
 * return value di eax */
static inline int do_syscall(uint32_t num, uint32_t arg1, uint32_t arg2, uint32_t arg3) {
    int ret;
    asm volatile(
        "int $0x80"
        : "=a" (ret)
        : "a" (num), "b" (arg1), "c" (arg2), "d" (arg3)
        : "memory"
    );
    return ret;
}

#endif
