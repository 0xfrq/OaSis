#ifndef STRING_H
#define STRING_H

#include <stdint.h>

int strcmp(const char*a, const char* b);
void itoa(int num, char* str, int base);
uint32_t strlen(const char *s);
void *memcpy(void *dest, const void *src, uint32_t n);
void *memmove(void *dest, const void *src, uint32_t n);
void *memset(void *dest, int val, uint32_t n);

#endif
