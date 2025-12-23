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