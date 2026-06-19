#ifndef KLIBC_H
#define KLIBC_H

/*
 * klibc.h - Kernel-space mini libc
 *
 * Fungsi-fungsi standard C library yang bisa dipanggil dari
 * program yang di-compile oleh occ (OaSis C Compiler).
 *
 * Semua fungsi pake prefix klibc_ secara internal, tapi
 * di-expose ke assembler sebagai _printf, _scanf, dll
 * lewat tabel extern di asm.c.
 */

#include <stdint.h>

/* Output */
int klibc_putchar(int c);
int klibc_puts(const char *s);
int klibc_printf(const char *fmt, ...);
int klibc_sprintf(char *buf, const char *fmt, ...);

/* Input */
int klibc_getchar(void);
char *klibc_gets(char *s);
int klibc_scanf(const char *fmt, ...);

/* String utility */
int klibc_atoi(const char *s);

/* Memory */
void *klibc_malloc(uint32_t size);
void  klibc_free(void *ptr);
void *klibc_calloc(uint32_t num, uint32_t size);
void *klibc_realloc(void *ptr, uint32_t new_size);

#endif
