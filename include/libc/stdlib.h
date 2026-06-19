#ifndef LIBC_STDLIB_H
#define LIBC_STDLIB_H

#include <stdint.h>

void exit(int status);

/* Konversi string ke number */
int atoi(const char *s);

/* Memory utility */
void *memset(void *dst, int c, uint32_t n);
void *memcpy(void *dst, const void *src, uint32_t n);
uint32_t strlen(const char *s);

#endif
