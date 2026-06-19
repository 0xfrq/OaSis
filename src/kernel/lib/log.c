/*
 * log.c - Kernel logging infrastructure
 *
 * Circular buffer untuk log messages.
 * Bisa dipanggil dari mana aja, aman buat interrupt context.
 * Format: [timestamp] message
 */

#include "log.h"
#include "vga.h"
#include "string.h"
#include "timer.h"
#include <stdarg.h>

/* Circular buffer */
static char log_buf[LOG_BUF_SIZE];
static volatile int write_pos = 0;
static volatile int read_pos = 0;

/* Flag: sudah di-init */
static int log_initialized = 0;

void log_init(void) {
    for (int i = 0; i < LOG_BUF_SIZE; i++) log_buf[i] = 0;
    write_pos = 0;
    read_pos = 0;
    log_initialized = 1;
    log_printf("[LOG] Logging initialized");
}

/* Tulis satu karakter ke buffer */
static void log_putc(char c) {
    if (!log_initialized) return;
    log_buf[write_pos] = c;
    write_pos = (write_pos + 1) % LOG_BUF_SIZE;
    /* Kalo overflow, majuin read_pos juga */
    if (write_pos == read_pos) {
        read_pos = (read_pos + 1) % LOG_BUF_SIZE;
    }
}

/* Tulis string ke buffer */
static void log_puts(const char *s) {
    while (s && *s) log_putc(*s++);
}

/* Konversi integer ke string di buffer */
static void log_int(int n, int base) {
    char tmp[32];
    int i = 0;
    if (n == 0) { log_putc('0'); return; }
    if (n < 0 && base == 10) { log_putc('-'); n = -n; }
    while (n > 0) {
        int d = n % base;
        tmp[i++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        n /= base;
    }
    while (i > 0) log_putc(tmp[--i]);
}

/* Konversi unsigned ke hex */
static void log_hex(uint32_t n) {
    log_puts("0x");
    char tmp[16];
    int i = 0;
    if (n == 0) { log_putc('0'); return; }
    while (n > 0) {
        int d = n % 16;
        tmp[i++] = (d < 10) ? ('0' + d) : ('a' + d - 10);
        n /= 16;
    }
    while (i > 0) log_putc(tmp[--i]);
}

/* log_printf — format: %s, %d, %x, %% */
void log_printf(const char *fmt, ...) {
    if (!fmt || !log_initialized) return;

    /* Tambah timestamp */
    uint32_t tick = timer_get_ticks();
    log_putc('[');
    log_int((int)tick, 10);
    log_putc(']');
    log_putc(' ');

    va_list args;
    va_start(args, fmt);

    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] != '%') {
            log_putc(fmt[i]);
            continue;
        }
        i++;
        switch (fmt[i]) {
            case 's': log_puts(va_arg(args, const char*)); break;
            case 'd': log_int(va_arg(args, int), 10); break;
            case 'x': log_hex(va_arg(args, uint32_t)); break;
            case '%': log_putc('%'); break;
            case '\0': goto done;
            default: log_putc('%'); log_putc(fmt[i]); break;
        }
    }

done:
    va_end(args);
    log_putc('\n');
}

/* Dump seluruh log ke VGA */
void log_dump(void) {
    vga_print("=== KERNEL LOG ===\n");
    int r = read_pos;
    int count = 0;
    while (r != write_pos && count < LOG_BUF_SIZE) {
        vga_putc(log_buf[r]);
        r = (r + 1) % LOG_BUF_SIZE;
        count++;
    }
    if (count == 0) {
        vga_print("(empty)\n");
    }
    vga_print("=== END LOG ===\n");
}

/* Log exception dengan detail */
void log_exception(int int_num, int err_code, uint32_t cr2, uint32_t eip) {
    log_printf("[EXC] int=%d err=0x%x cr2=0x%x eip=0x%x",
               int_num, err_code, cr2, eip);
}
