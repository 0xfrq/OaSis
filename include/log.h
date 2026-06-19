#ifndef LOG_H
#define LOG_H

#include <stdint.h>

/* Ukuran circular buffer log */
#define LOG_BUF_SIZE 4096

/* Inisialisasi logging */
void log_init(void);

/* Log printf — simpan pesan ke buffer */
void log_printf(const char *fmt, ...);

/* Tampilkan seluruh isi log */
void log_dump(void);

/* Log exception dengan detail */
void log_exception(int int_num, int err_code, uint32_t cr2, uint32_t eip);

#endif
