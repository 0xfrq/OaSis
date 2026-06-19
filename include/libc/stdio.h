#ifndef LIBC_STDIO_H
#define LIBC_STDIO_H

#include <stdint.h>

/* File descriptor standar */
#define stdin  0
#define stdout 1
#define stderr 2

/* ====== Output ====== */
int putchar(int c);
int puts(const char *s);
int print_int(int n);
int print_hex(uint32_t n);
int printf(const char *format, ...);

/* ====== Input ====== */
int getchar(void);
char *gets(char *s);
int scanf(const char *format, ...);

/* ====== File I/O ====== */
int open(const char *path, int flags);
int close(int fd);
int read(int fd, void *buf, uint32_t count);
int write(int fd, const void *buf, uint32_t count);

#endif
