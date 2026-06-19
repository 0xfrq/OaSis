/*
 * klibc.c - Kernel-space mini libc
 *
 * Implementasi fungsi standard C library (printf, scanf, putchar, dll)
 * yang bisa dipanggil dari program yang di-compile oleh occ.
 *
 * Fungsi-fungsi ini jalan di kernel mode, pake vga_putc/vga_print
 * untuk output dan keyboard_getchar untuk input.
 */

#include "klibc.h"
#include "vga.h"
#include "keyboard.h"
#include "string.h"
#include "heap.h"
#include <stdarg.h>

/* ====== Output ====== */

int klibc_putchar(int c) {
    vga_putc((char)c);
    return c;
}

int klibc_puts(const char *s) {
    if (!s) s = "(null)";
    vga_print(s);
    vga_putc('\n');
    return 0;
}

/* ====== printf internal helpers ====== */

/* print unsigned integer ke VGA, return jumlah karakter */
static int print_unsigned(unsigned int n, int base, int uppercase) {
    if (n == 0) {
        vga_putc('0');
        return 1;
    }
    char buf[32];
    int i = 0;
    while (n > 0) {
        int rem = n % base;
        if (rem < 10)
            buf[i++] = (char)('0' + rem);
        else
            buf[i++] = (char)((uppercase ? 'A' : 'a') + rem - 10);
        n /= (unsigned)base;
    }
    int count = i;
    while (i > 0) vga_putc(buf[--i]);
    return count;
}

/* print signed integer ke VGA, return jumlah karakter */
static int print_signed(int n) {
    int count = 0;
    if (n < 0) {
        vga_putc('-');
        count++;
        /* handle INT_MIN: can't negate directly */
        if (n == (int)0x80000000) {
            /* -2147483648 */
            vga_print("2147483648");
            return 11;
        }
        n = -n;
    }
    count += print_unsigned((unsigned int)n, 10, 0);
    return count;
}

/* padding helper */
static int emit_padding(int count, char pad_char) {
    for (int i = 0; i < count; i++)
        vga_putc(pad_char);
    return count;
}

/* ====== printf ====== */

int klibc_printf(const char *fmt, ...) {
    if (!fmt) return 0;

    va_list args;
    va_start(args, fmt);
    int count = 0;

    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] != '%') {
            vga_putc(fmt[i]);
            count++;
            continue;
        }

        /* parse format specifier */
        i++;
        int left_align = 0;
        int pad_zero = 0;
        int width = 0;

        /* flags */
        if (fmt[i] == '-') { left_align = 1; i++; }
        if (fmt[i] == '0' && !left_align) { pad_zero = 1; i++; }

        /* width */
        while (fmt[i] >= '0' && fmt[i] <= '9') {
            width = width * 10 + (fmt[i] - '0');
            i++;
        }

        /* specifier */
        switch (fmt[i]) {
            case 'd':
            case 'i': {
                int val = va_arg(args, int);
                /* calculate length first for padding */
                int len = 0;
                int tmp = val;
                if (tmp < 0) { len++; tmp = -tmp; }
                if (tmp == 0) len = 1;
                else { while (tmp > 0) { len++; tmp /= 10; } }

                if (width > len && !left_align)
                    count += emit_padding(width - len, pad_zero ? '0' : ' ');
                count += print_signed(val);
                if (width > len && left_align)
                    count += emit_padding(width - len, ' ');
                break;
            }
            case 'u': {
                unsigned int val = va_arg(args, unsigned int);
                int len = 0;
                unsigned int tmp = val;
                if (tmp == 0) len = 1;
                else { while (tmp > 0) { len++; tmp /= 10; } }

                if (width > len && !left_align)
                    count += emit_padding(width - len, pad_zero ? '0' : ' ');
                count += print_unsigned(val, 10, 0);
                if (width > len && left_align)
                    count += emit_padding(width - len, ' ');
                break;
            }
            case 'x': {
                unsigned int val = va_arg(args, unsigned int);
                int len = 0;
                unsigned int tmp = val;
                if (tmp == 0) len = 1;
                else { while (tmp > 0) { len++; tmp /= 16; } }

                if (width > len && !left_align)
                    count += emit_padding(width - len, pad_zero ? '0' : ' ');
                count += print_unsigned(val, 16, 0);
                if (width > len && left_align)
                    count += emit_padding(width - len, ' ');
                break;
            }
            case 'X': {
                unsigned int val = va_arg(args, unsigned int);
                int len = 0;
                unsigned int tmp = val;
                if (tmp == 0) len = 1;
                else { while (tmp > 0) { len++; tmp /= 16; } }

                if (width > len && !left_align)
                    count += emit_padding(width - len, pad_zero ? '0' : ' ');
                count += print_unsigned(val, 16, 1);
                if (width > len && left_align)
                    count += emit_padding(width - len, ' ');
                break;
            }
            case 'o': {
                unsigned int val = va_arg(args, unsigned int);
                int len = 0;
                unsigned int tmp = val;
                if (tmp == 0) len = 1;
                else { while (tmp > 0) { len++; tmp /= 8; } }

                if (width > len && !left_align)
                    count += emit_padding(width - len, pad_zero ? '0' : ' ');
                count += print_unsigned(val, 8, 0);
                if (width > len && left_align)
                    count += emit_padding(width - len, ' ');
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                if (width > 1 && !left_align)
                    count += emit_padding(width - 1, ' ');
                vga_putc(c);
                count++;
                if (width > 1 && left_align)
                    count += emit_padding(width - 1, ' ');
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                int len = (int)strlen(s);
                if (width > len && !left_align)
                    count += emit_padding(width - len, ' ');
                vga_print(s);
                count += len;
                if (width > len && left_align)
                    count += emit_padding(width - len, ' ');
                break;
            }
            case 'p': {
                unsigned int val = (unsigned int)va_arg(args, void *);
                vga_putc('0');
                vga_putc('x');
                count += 2;
                count += print_unsigned(val, 16, 0);
                break;
            }
            case '%':
                vga_putc('%');
                count++;
                break;
            case '\0':
                /* premature end of format string */
                goto done;
            default:
                vga_putc('%');
                vga_putc(fmt[i]);
                count += 2;
                break;
        }
    }

done:
    va_end(args);
    return count;
}

/* ====== Input ====== */

int klibc_getchar(void) {
    char c = keyboard_getchar();
    vga_putc(c);  /* echo */
    return (int)(unsigned char)c;
}

char *klibc_gets(char *s) {
    int i = 0;
    while (1) {
        char c = keyboard_getchar();
        if (c == '\n') {
            vga_putc('\n');
            break;
        }
        if (c == '\b') {
            if (i > 0) {
                i--;
                vga_putc('\b');
            }
            continue;
        }
        vga_putc(c);
        s[i++] = c;
    }
    s[i] = '\0';
    return s;
}

/* skip whitespace helper */
static void skip_ws(const char *line, int *pos) {
    while (line[*pos] == ' ' || line[*pos] == '\t')
        (*pos)++;
}

int klibc_scanf(const char *fmt, ...) {
    if (!fmt) return 0;

    va_list args;
    va_start(args, fmt);

    /* baca satu baris input dulu */
    char line[256];
    klibc_gets(line);
    int lpos = 0;
    int matched = 0;

    for (int i = 0; fmt[i] != '\0'; i++) {
        /* whitespace di format -> skip whitespace di input */
        if (fmt[i] == ' ' || fmt[i] == '\t') {
            skip_ws(line, &lpos);
            continue;
        }

        if (fmt[i] != '%') {
            /* literal match */
            if (line[lpos] == fmt[i])
                lpos++;
            else
                break; /* mismatch */
            continue;
        }

        /* format specifier */
        i++;
        if (fmt[i] == '\0') break;

        /* optional width */
        int max_width = 0;
        while (fmt[i] >= '0' && fmt[i] <= '9') {
            max_width = max_width * 10 + (fmt[i] - '0');
            i++;
        }

        switch (fmt[i]) {
            case 'd': {
                int *val = va_arg(args, int *);
                skip_ws(line, &lpos);
                if (line[lpos] == '\0') goto done_scanf;
                int sign = 1;
                int num = 0;
                int digits = 0;
                if (line[lpos] == '-') { sign = -1; lpos++; }
                else if (line[lpos] == '+') { lpos++; }
                while (line[lpos] >= '0' && line[lpos] <= '9') {
                    if (max_width > 0 && digits >= max_width) break;
                    num = num * 10 + (line[lpos] - '0');
                    lpos++;
                    digits++;
                }
                if (digits == 0) goto done_scanf;
                *val = num * sign;
                matched++;
                break;
            }
            case 'u': {
                unsigned int *val = va_arg(args, unsigned int *);
                skip_ws(line, &lpos);
                if (line[lpos] == '\0') goto done_scanf;
                unsigned int num = 0;
                int digits = 0;
                while (line[lpos] >= '0' && line[lpos] <= '9') {
                    if (max_width > 0 && digits >= max_width) break;
                    num = num * 10 + (unsigned)(line[lpos] - '0');
                    lpos++;
                    digits++;
                }
                if (digits == 0) goto done_scanf;
                *val = num;
                matched++;
                break;
            }
            case 'x': {
                unsigned int *val = va_arg(args, unsigned int *);
                skip_ws(line, &lpos);
                if (line[lpos] == '\0') goto done_scanf;
                /* skip optional 0x prefix */
                if (line[lpos] == '0' && (line[lpos+1] == 'x' || line[lpos+1] == 'X'))
                    lpos += 2;
                unsigned int num = 0;
                int digits = 0;
                while (1) {
                    if (max_width > 0 && digits >= max_width) break;
                    char ch = line[lpos];
                    if (ch >= '0' && ch <= '9')      { num = num * 16 + (unsigned)(ch - '0'); }
                    else if (ch >= 'a' && ch <= 'f')  { num = num * 16 + (unsigned)(ch - 'a' + 10); }
                    else if (ch >= 'A' && ch <= 'F')  { num = num * 16 + (unsigned)(ch - 'A' + 10); }
                    else break;
                    lpos++;
                    digits++;
                }
                if (digits == 0) goto done_scanf;
                *val = num;
                matched++;
                break;
            }
            case 'o': {
                unsigned int *val = va_arg(args, unsigned int *);
                skip_ws(line, &lpos);
                if (line[lpos] == '\0') goto done_scanf;
                unsigned int num = 0;
                int digits = 0;
                while (line[lpos] >= '0' && line[lpos] <= '7') {
                    if (max_width > 0 && digits >= max_width) break;
                    num = num * 8 + (unsigned)(line[lpos] - '0');
                    lpos++;
                    digits++;
                }
                if (digits == 0) goto done_scanf;
                *val = num;
                matched++;
                break;
            }
            case 's': {
                char *str = va_arg(args, char *);
                skip_ws(line, &lpos);
                int j = 0;
                while (line[lpos] && line[lpos] != ' ' && line[lpos] != '\t' && line[lpos] != '\n') {
                    if (max_width > 0 && j >= max_width) break;
                    str[j++] = line[lpos++];
                }
                str[j] = '\0';
                if (j > 0) matched++;
                break;
            }
            case 'c': {
                char *c = va_arg(args, char *);
                if (line[lpos] != '\0') {
                    *c = line[lpos++];
                    matched++;
                }
                break;
            }
        }
    }

done_scanf:
    va_end(args);
    return matched;
}

/* ====== String utility ====== */

int klibc_atoi(const char *s) {
    int sign = 1, num = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') { s++; }
    while (*s >= '0' && *s <= '9') {
        num = num * 10 + (*s - '0');
        s++;
    }
    return num * sign;
}

/* sprintf - print ke buffer, bukan ke VGA */
int klibc_sprintf(char *buf, const char *fmt, ...) {
    if (!fmt || !buf) return 0;

    va_list args;
    va_start(args, fmt);
    int pos = 0;

    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] != '%') {
            buf[pos++] = fmt[i];
            continue;
        }
        i++;
        switch (fmt[i]) {
            case 'd':
            case 'i': {
                int val = va_arg(args, int);
                char tmp[16];
                itoa(val, tmp, 10);
                for (int j = 0; tmp[j]; j++)
                    buf[pos++] = tmp[j];
                break;
            }
            case 'u': {
                unsigned int val = va_arg(args, unsigned int);
                /* manual unsigned to string */
                char tmp[16];
                int ti = 0;
                if (val == 0) { tmp[ti++] = '0'; }
                else {
                    while (val > 0) {
                        tmp[ti++] = (char)('0' + val % 10);
                        val /= 10;
                    }
                }
                /* reverse */
                for (int j = ti - 1; j >= 0; j--)
                    buf[pos++] = tmp[j];
                break;
            }
            case 'x': {
                unsigned int val = va_arg(args, unsigned int);
                char tmp[16];
                itoa((int)val, tmp, 16);
                for (int j = 0; tmp[j]; j++)
                    buf[pos++] = tmp[j];
                break;
            }
            case 's': {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                while (*s) buf[pos++] = *s++;
                break;
            }
            case 'c':
                buf[pos++] = (char)va_arg(args, int);
                break;
            case '%':
                buf[pos++] = '%';
                break;
            case '\0':
                goto done_sprintf;
            default:
                buf[pos++] = '%';
                buf[pos++] = fmt[i];
                break;
        }
    }

done_sprintf:
    buf[pos] = '\0';
    va_end(args);
    return pos;
}

/* ====== Memory allocation ====== */

void *klibc_malloc(uint32_t size) {
    return kmalloc(size);
}

void klibc_free(void *ptr) {
    kfree(ptr);
}

void *klibc_calloc(uint32_t num, uint32_t size) {
    return kcalloc(num, size);
}

void *klibc_realloc(void *ptr, uint32_t new_size) {
    return krealloc(ptr, new_size);
}
