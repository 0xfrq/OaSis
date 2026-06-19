#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

/* kode tombol spesial buat yang bukan karakter biasa */
#define KEY_ARROW_UP    0x81
#define KEY_ARROW_DOWN  0x82
#define KEY_ARROW_LEFT  0x83
#define KEY_ARROW_RIGHT 0x84
#define KEY_CTRL_S      0x85
#define KEY_CTRL_X      0x86
#define KEY_DELETE      0x87
#define KEY_HOME        0x88
#define KEY_END         0x89
#define KEY_PGUP        0x8A
#define KEY_PGDN        0x8B
#define KEY_CTRL_Z      0x8C
#define KEY_NONE        0x00

/* modifier keys */
#define KEY_LSHIFT      0x90
#define KEY_RSHIFT      0x91

void keyboard_init(void);
void keyboard_interrupt_handler(void);
char keyboard_getchar(void);
uint8_t keyboard_getkey(void);
void keyboard_flush(void);  /* reset buffer keyboard */

#endif
