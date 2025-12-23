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

    // Reverse the string
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