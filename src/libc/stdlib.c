#include <libc/stdlib.h>
#include <libc/syscall.h>

void exit(int status) {
    do_syscall(SYS_EXIT, status, 0, 0);
    while (1); /* Halt jika syscall return */
}

uint32_t strlen(const char *s) {
    uint32_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}
