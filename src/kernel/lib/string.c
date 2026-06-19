#include "string.h"

int strcmp(const char* a, const char* b) {
    int i = 0;
    while (a[i] && b[i]) {
        if(a[i] != b[i])
            return a[i] - b[i];
        i++;
    }
    return a[i] - b[i];
}

void itoa(int num, char* str, int base) {
    int i = 0;
    int negative = 0;

    if (num == 0) {
        str[i++] = '0';
        str[i] = 0;
        return;
    }

    if (num < 0 && base == 10) {
        negative = 1;
        num = -num;
    }

    while (num > 0) {
        int digit = num % base;
        str[i++] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        num /= base;
    }

    if (negative) {
        str[i++] = '-';
    }

    str[i] = 0;

    // Balikin string-nya
    int start = 0;
    int end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

uint32_t strlen(const char *s) {
    uint32_t len = 0;
    while (s[len]) len++;
    return len;
}

void *memcpy(void *dest, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void *memmove(void *dest, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    if (d < s) {
        for (uint32_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else if (d > s) {
        for (uint32_t i = n; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

void *memset(void *dest, int val, uint32_t n) {
    uint8_t *d = (uint8_t *)dest;
    uint8_t v = (uint8_t)val;
    for (uint32_t i = 0; i < n; i++) {
        d[i] = v;
    }
    return dest;
}