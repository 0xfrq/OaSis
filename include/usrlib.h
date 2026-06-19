#ifndef USRLIB_H
#define USRLIB_H

#include <stdint.h>

/* User space library — fungsi yang pake syscall (int 0x80) */

/* Output */
int usr_putchar(int c);
int usr_puts(const char *s);
int usr_printf(const char *fmt, ...);

/* Input */
int usr_getchar(void);
char *usr_gets(char *s);

/* Memory */
void *usr_malloc(uint32_t size);
void usr_free(void *ptr);

#endif
