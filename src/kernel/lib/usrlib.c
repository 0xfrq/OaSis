/*
 * usrlib.c - User space library via syscalls
 *
 * Fungsi-fungsi ini bisa dipanggil dari program ring 3 (user mode).
 * Semua I/O pake int 0x80 syscall.
 *
 * Dipanggil dari built-in assembler via external symbol.
 * Fungsi: usr_printf, usr_putchar, usr_gets, usr_malloc, dll.
 *
 * NOTE: Fungsi ini TIDAK otomatis ter-prefix _.
 * Prefix _ ditambah manual di extern_syms[] asm.c.
 */

#include "syscall.h"
#include <stdarg.h>
#include <stdint.h>

/* Syscall wrappers */
static inline uint32_t sys0(uint32_t num) {
    register uint32_t eax asm("eax") = num;
    asm volatile("int $0x80" : "+r"(eax) : : "memory");
    return eax;
}

static inline uint32_t sys1(uint32_t num, uint32_t a1) {
    register uint32_t eax asm("eax") = num;
    register uint32_t ebx asm("ebx") = a1;
    asm volatile("int $0x80" : "+r"(eax) : "r"(ebx) : "memory");
    return eax;
}

static inline uint32_t sys2(uint32_t num, uint32_t a1, uint32_t a2) {
    register uint32_t eax asm("eax") = num;
    register uint32_t ebx asm("ebx") = a1;
    register uint32_t ecx asm("ecx") = a2;
    asm volatile("int $0x80" : "+r"(eax) : "r"(ebx), "r"(ecx) : "memory");
    return eax;
}

static inline uint32_t sys3(uint32_t num, uint32_t a1, uint32_t a2, uint32_t a3) {
    register uint32_t eax asm("eax") = num;
    register uint32_t ebx asm("ebx") = a1;
    register uint32_t ecx asm("ecx") = a2;
    register uint32_t edx asm("edx") = a3;
    asm volatile("int $0x80" : "+r"(eax) : "r"(ebx), "r"(ecx), "r"(edx) : "memory");
    return eax;
}

/* ====== Output ====== */

int usr_putchar(int c) {
    char ch = (char)c;
    /* write to stdout via syscall */
    sys3(SYSCALL_WRITE_FD, 1, (uint32_t)&ch, 1);
    return c;
}

int usr_puts(const char *s) {
    int len = 0;
    while (s && s[len]) len++;
    sys3(SYSCALL_WRITE_FD, 1, (uint32_t)s, (uint32_t)len);
    usr_putchar('\n');
    return len;
}

int usr_printf(const char *fmt, ...) {
    if (!fmt) return 0;
    int count = 0;
    va_list args;
    va_start(args, fmt);

    for (int i = 0; fmt[i]; i++) {
        if (fmt[i] != '%') {
            usr_putchar(fmt[i]);
            count++;
            continue;
        }
        i++;
        switch (fmt[i]) {
            case 'd': case 'i': {
                int v = va_arg(args, int);
                /* convert int to string, print */
                if (v < 0) { usr_putchar('-'); v = -v; count++; }
                char buf[16]; int bi = 0;
                if (v == 0) { usr_putchar('0'); count++; break; }
                while (v > 0) { buf[bi++] = (char)('0' + v % 10); v /= 10; }
                while (bi > 0) { usr_putchar(buf[--bi]); count++; }
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                while (*s) { usr_putchar(*s++); count++; }
                break;
            }
            case 'c': {
                usr_putchar(va_arg(args, int));
                count++;
                break;
            }
            case 'x': {
                unsigned int v = (unsigned int)va_arg(args, unsigned int);
                char buf[16]; int bi = 0;
                if (v == 0) { usr_putchar('0'); count++; break; }
                while (v > 0) {
                    int d = v % 16;
                    buf[bi++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
                    v /= 16;
                }
                while (bi > 0) { usr_putchar(buf[--bi]); count++; }
                break;
            }
            case 'u': {
                unsigned int v = va_arg(args, unsigned int);
                char buf[16]; int bi = 0;
                if (v == 0) { usr_putchar('0'); count++; break; }
                while (v > 0) { buf[bi++] = (char)('0' + v % 10); v /= 10; }
                while (bi > 0) { usr_putchar(buf[--bi]); count++; }
                break;
            }
            case '%': usr_putchar('%'); count++; break;
            default: usr_putchar('%'); usr_putchar(fmt[i]); count += 2; break;
        }
    }
    va_end(args);
    return count;
}

/* ====== Input ====== */

int usr_getchar(void) {
    char c;
    if (sys3(SYSCALL_READ, 0, (uint32_t)&c, 1) <= 0) return -1;
    return (int)(unsigned char)c;
}

char *usr_gets(char *s) {
    int i = 0, c;
    while ((c = usr_getchar()) != '\n' && c != -1) {
        if (c == '\b' && i > 0) i--;
        else s[i++] = (char)c;
    }
    s[i] = '\0';
    return s;
}

/* ====== Memory ====== */

void *usr_malloc(uint32_t size) {
    /* Use brk syscall to allocate memory */
    uint32_t cur = sys1(SYSCALL_BRK, 0);
    if (cur == 0) return 0;
    uint32_t new = sys1(SYSCALL_BRK, cur + size + 4);
    if (new <= cur) return 0;
    /* Return pointer (as int) */
    return (void *)(cur + 4);
}

void usr_free(void *ptr) {
    /* Free-list management not available from user mode yet */
    (void)ptr;
}
