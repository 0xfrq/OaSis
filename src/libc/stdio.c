#include <libc/stdio.h>
#include <libc/stdlib.h>
#include <libc/syscall.h>
#include <stdarg.h>

/* ====== Output ====== */

int putchar(int c) {
    char ch = (char)c;
    return do_syscall(SYS_WRITE, (uint32_t)&ch, 1, 0);
}

int puts(const char *s) {
    int len = strlen(s);
    int ret = do_syscall(SYS_WRITE, (uint32_t)s, len, 0);
    putchar('\n');
    return ret;
}

int print_int(int n) {
    if (n == 0) {
        putchar('0');
        return 1;
    }

    int count = 0;
    if (n < 0) {
        putchar('-');
        n = -n;
        count++;
    }

    char buf[16];
    int i = 0;
    while (n > 0) {
        buf[i++] = (n % 10) + '0';
        n /= 10;
    }

    count += i;
    while (i > 0) {
        putchar(buf[--i]);
    }
    return count;
}

int print_hex(uint32_t n) {
    putchar('0');
    putchar('x');
    if (n == 0) {
        putchar('0');
        return 3;
    }

    char buf[16];
    int i = 0;
    while (n > 0) {
        int rem = n % 16;
        buf[i++] = (rem < 10) ? (rem + '0') : (rem - 10 + 'a');
        n /= 16;
    }

    int count = i + 2;
    while (i > 0) {
        putchar(buf[--i]);
    }
    return count;
}

/* Helper function to print a string with padding */
static int print_padded_string(const char *str, int width, int pad_zero, int left_align) {
    int len = strlen(str);
    int count = 0;

    if (width > len && !left_align) {
        // Pad on the left
        for (int i = 0; i < width - len; i++) {
            putchar(pad_zero ? '0' : ' ');
            count++;
        }
    }

    // Print the string
    for (int i = 0; str[i] != '\0'; i++) {
        putchar(str[i]);
        count++;
    }

    if (width > len && left_align) {
        // Pad on the right
        for (int i = 0; i < width - len; i++) {
            putchar(' ');
            count++;
        }
    }

    return count;
}

/* Helper function to convert integer to string with specified base */
static int int_to_string(char *buf, int n, int base, int uppercase) {
    if (n == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }

    int i = 0;
    int is_negative = 0;

    if (n < 0 && base == 10) {
        is_negative = 1;
        n = -n;
    }

    while (n > 0) {
        int rem = n % base;
        if (rem < 10) {
            buf[i++] = rem + '0';
        } else {
            buf[i++] = (rem - 10) + (uppercase ? 'A' : 'a');
        }
        n /= base;
    }

    if (is_negative) {
        buf[i++] = '-';
    }

    // Reverse the string
    for (int j = 0; j < i / 2; j++) {
        char temp = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = temp;
    }

    buf[i] = '\0';
    return i;
}

/* printf - enhanced version with support for %d %s %c %x %u %o %p %% and formatting */
int printf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    int count = 0;
    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%') {
            // Parse format specifiers
            int width = 0;
            int pad_zero = 0;
            int left_align = 0;

            i++;

            // Check for left alignment
            if (format[i] == '-') {
                left_align = 1;
                i++;
            }

            // Check for zero padding
            if (format[i] == '0') {
                pad_zero = 1;
                i++;
            }

            // Parse width
            while (format[i] >= '0' && format[i] <= '9') {
                width = width * 10 + (format[i] - '0');
                i++;
            }

            switch (format[i]) {
                case 'c': {
                    char c = (char)va_arg(args, int);
                    char str[2] = {c, '\0'};
                    count += print_padded_string(str, width, 0, left_align);
                    break;
                }
                case 's': {
                    char *str = va_arg(args, char*);
                    if (str == NULL) str = "(null)";
                    count += print_padded_string(str, width, 0, left_align);
                    break;
                }
                case 'd':
                case 'i': {
                    int n = va_arg(args, int);
                    char buf[32];
                    int len = int_to_string(buf, n, 10, 0);
                    count += print_padded_string(buf, width, pad_zero, left_align);
                    break;
                }
                case 'u': {
                    unsigned int u = va_arg(args, unsigned int);
                    char buf[32];
                    if (u == 0) {
                        buf[0] = '0';
                        buf[1] = '\0';
                        len = 1;
                    } else {
                        int len = 0;
                        unsigned int temp = u;
                        while (temp > 0) {
                            buf[len++] = (temp % 10) + '0';
                            temp /= 10;
                        }
                        // Reverse the string
                        for (int j = 0; j < len / 2; j++) {
                            char temp_char = buf[j];
                            buf[j] = buf[len - 1 - j];
                            buf[len - 1 - j] = temp_char;
                        }
                        buf[len] = '\0';
                    }
                    count += print_padded_string(buf, width, pad_zero, left_align);
                    break;
                }
                case 'x': {
                    unsigned int x = va_arg(args, unsigned int);
                    char buf[32];
                    if (x == 0) {
                        buf[0] = '0';
                        buf[1] = '\0';
                        len = 1;
                    } else {
                        int len = 0;
                        unsigned int temp = x;
                        while (temp > 0) {
                            int rem = temp % 16;
                            buf[len++] = (rem < 10) ? (rem + '0') : (rem - 10 + 'a');
                            temp /= 16;
                        }
                        // Reverse the string
                        for (int j = 0; j < len / 2; j++) {
                            char temp_char = buf[j];
                            buf[j] = buf[len - 1 - j];
                            buf[len - 1 - j] = temp_char;
                        }
                        buf[len] = '\0';
                    }
                    count += print_padded_string(buf, width, pad_zero, left_align);
                    break;
                }
                case 'X': {
                    unsigned int x = va_arg(args, unsigned int);
                    char buf[32];
                    if (x == 0) {
                        buf[0] = '0';
                        buf[1] = '\0';
                        len = 1;
                    } else {
                        int len = 0;
                        unsigned int temp = x;
                        while (temp > 0) {
                            int rem = temp % 16;
                            buf[len++] = (rem < 10) ? (rem + '0') : (rem - 10 + 'A');
                            temp /= 16;
                        }
                        // Reverse the string
                        for (int j = 0; j < len / 2; j++) {
                            char temp_char = buf[j];
                            buf[j] = buf[len - 1 - j];
                            buf[len - 1 - j] = temp_char;
                        }
                        buf[len] = '\0';
                    }
                    count += print_padded_string(buf, width, pad_zero, left_align);
                    break;
                }
                case 'o': {
                    unsigned int o = va_arg(args, unsigned int);
                    char buf[32];
                    if (o == 0) {
                        buf[0] = '0';
                        buf[1] = '\0';
                        len = 1;
                    } else {
                        int len = 0;
                        unsigned int temp = o;
                        while (temp > 0) {
                            buf[len++] = (temp % 8) + '0';
                            temp /= 8;
                        }
                        // Reverse the string
                        for (int j = 0; j < len / 2; j++) {
                            char temp_char = buf[j];
                            buf[j] = buf[len - 1 - j];
                            buf[len - 1 - j] = temp_char;
                        }
                        buf[len] = '\0';
                    }
                    count += print_padded_string(buf, width, pad_zero, left_align);
                    break;
                }
                case 'p': {
                    void *p = va_arg(args, void*);
                    unsigned int addr = (unsigned int)(uintptr_t)p;
                    char buf[32];
                    buf[0] = '0';
                    buf[1] = 'x';
                    int len = 2;
                    if (addr == 0) {
                        buf[len++] = '0';
                    } else {
                        int start = len;
                        unsigned int temp = addr;
                        while (temp > 0) {
                            int rem = temp % 16;
                            buf[len++] = (rem < 10) ? (rem + '0') : (rem - 10 + 'a');
                            temp /= 16;
                        }
                        // Reverse the hex part
                        for (int j = 0; j < (len - start) / 2; j++) {
                            char temp_char = buf[start + j];
                            buf[start + j] = buf[len - 1 - j];
                            buf[len - 1 - j] = temp_char;
                        }
                    }
                    buf[len] = '\0';
                    count += print_padded_string(buf, width, 0, left_align);
                    break;
                }
                case '%':
                    putchar('%');
                    count++;
                    break;
                default:
                    putchar('%');
                    putchar(format[i]);
                    count += 2;
                    break;
            }
        } else {
            putchar(format[i]);
            count++;
        }
    }

    va_end(args);
    return count;
}

/* ====== Input ====== */

int getchar(void) {
    char c;
    int n = do_syscall(SYS_READ, stdin, (uint32_t)&c, 1);
    if (n <= 0) return -1;
    return (int)c;
}

char *gets(char *s) {
    int i = 0;
    int c;
    while ((c = getchar()) != '\n' && c != -1) {
        if (c == '\b') {
            if (i > 0) i--;
            continue;
        }
        s[i++] = (char)c;
    }
    s[i] = '\0';
    return s;
}

/* Helper function to skip whitespace in input */
static void skip_whitespace(char *line, int *pos) {
    while (line[*pos] == ' ' || line[*pos] == '\t') {
        (*pos)++;
    }
}

/* Helper function to parse integer with specified base */
static int parse_int(char *line, int *pos, int base) {
    int sign = 1;
    int num = 0;

    // Skip whitespace
    skip_whitespace(line, pos);

    // Check for sign
    if (line[*pos] == '-') {
        sign = -1;
        (*pos)++;
    } else if (line[*pos] == '+') {
        (*pos)++;
    }

    // Parse number based on base
    while (1) {
        char ch = line[*pos];
        int digit = -1;

        if (ch >= '0' && ch <= '9') {
            digit = ch - '0';
        } else if (ch >= 'a' && ch <= 'f' && base > 10) {
            digit = ch - 'a' + 10;
        } else if (ch >= 'A' && ch <= 'F' && base > 10) {
            digit = ch - 'A' + 10;
        }

        if (digit >= 0 && digit < base) {
            num = num * base + digit;
            (*pos)++;
        } else {
            break;
        }
    }

    return num * sign;
}

/* scanf - enhanced version with support for %d %s %c %x %o %p and improved parsing */
int scanf(const char *format, ...) {
    va_list args;
    va_start(args, format);

    int matched = 0;
    char line[256];
    gets(line);
    int line_pos = 0;

    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%') {
            i++;

            // Parse width (if specified)
            int width = 0;
            while (format[i] >= '0' && format[i] <= '9') {
                width = width * 10 + (format[i] - '0');
                i++;
            }

            switch (format[i]) {
                case 'd': {
                    int *val = va_arg(args, int*);
                    skip_whitespace(line, &line_pos);
                    if (line[line_pos] != '\0') {
                        *val = parse_int(line, &line_pos, 10);
                        matched++;
                    }
                    break;
                }
                case 's': {
                    char *str = va_arg(args, char*);
                    skip_whitespace(line, &line_pos);
                    int j = 0;
                    // If width is specified, use it; otherwise read until whitespace
                    if (width > 0) {
                        while (line[line_pos] && line[line_pos] != ' ' && line[line_pos] != '\t' && line[line_pos] != '\n' && j < width) {
                            str[j++] = line[line_pos++];
                        }
                    } else {
                        while (line[line_pos] && line[line_pos] != ' ' && line[line_pos] != '\t' && line[line_pos] != '\n') {
                            str[j++] = line[line_pos++];
                        }
                    }
                    str[j] = '\0';
                    if (j > 0) matched++;
                    break;
                }
                case 'c': {
                    char *c = va_arg(args, char*);
                    if (width > 0) {
                        // Read width characters (including whitespace)
                        for (int k = 0; k < width && line[line_pos] != '\0'; k++) {
                            c[k] = line[line_pos++];
                        }
                        matched++;
                    } else {
                        // Read single character
                        *c = line[line_pos++];
                        matched++;
                    }
                    break;
                }
                case 'x': {
                    uint32_t *val = va_arg(args, uint32_t*);
                    skip_whitespace(line, &line_pos);
                    if (line[line_pos] != '\0') {
                        uint32_t num = 0;
                        // Skip optional 0x prefix
                        if (line[line_pos] == '0' && (line[line_pos+1] == 'x' || line[line_pos+1] == 'X')) {
                            line_pos += 2;
                        }
                        while (1) {
                            char ch = line[line_pos];
                            if (ch >= '0' && ch <= '9') { num = num * 16 + (ch - '0'); line_pos++; }
                            else if (ch >= 'a' && ch <= 'f') { num = num * 16 + (ch - 'a' + 10); line_pos++; }
                            else if (ch >= 'A' && ch <= 'F') { num = num * 16 + (ch - 'A' + 10); line_pos++; }
                            else break;
                        }
                        *val = num;
                        matched++;
                    }
                    break;
                }
                case 'o': {
                    uint32_t *val = va_arg(args, uint32_t*);
                    skip_whitespace(line, &line_pos);
                    if (line[line_pos] != '\0') {
                        uint32_t num = 0;
                        while (line[line_pos] >= '0' && line[line_pos] <= '7') {
                            num = num * 8 + (line[line_pos] - '0');
                            line_pos++;
                        }
                        *val = num;
                        matched++;
                    }
                    break;
                }
                case 'p': {
                    void **val = va_arg(args, void**);
                    skip_whitespace(line, &line_pos);
                    if (line[line_pos] != '\0') {
                        // Skip 0x prefix if present
                        if (line[line_pos] == '0' && (line[line_pos+1] == 'x' || line[line_pos+1] == 'X')) {
                            line_pos += 2;
                        }
                        uintptr_t addr = 0;
                        while (1) {
                            char ch = line[line_pos];
                            if (ch >= '0' && ch <= '9') { addr = addr * 16 + (ch - '0'); line_pos++; }
                            else if (ch >= 'a' && ch <= 'f') { addr = addr * 16 + (ch - 'a' + 10); line_pos++; }
                            else if (ch >= 'A' && ch <= 'F') { addr = addr * 16 + (ch - 'A' + 10); line_pos++; }
                            else break;
                        }
                        *val = (void*)addr;
                        matched++;
                    }
                    break;
                }
            }
        }
    }

    va_end(args);
    return matched;
}

/* ====== File I/O ====== */

int open(const char *path, int flags) {
    /* SYS_OPEN = 9 */
    return do_syscall(9, (uint32_t)path, (uint32_t)flags, 0);
}

int close(int fd) {
    /* SYS_CLOSE = 10 */
    return do_syscall(10, (uint32_t)fd, 0, 0);
}

int read(int fd, void *buf, uint32_t count) {
    return do_syscall(SYS_READ, (uint32_t)fd, (uint32_t)buf, count);
}

int write(int fd, const void *buf, uint32_t count) {
    return do_syscall(SYS_WRITE, (uint32_t)fd, (uint32_t)buf, count);
}
