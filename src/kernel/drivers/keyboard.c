#include "keyboard.h"
#include "io.h"
#include "vga.h"
#include <stdint.h>

#define KEYBOARD_DATA 0x60
#define KEYBOARD_STATUS 0x64
#define KEYBOARD_BUFFER_SIZE 256

/* keymap US (tanpa shift) */
static const char keymap_us[128] = {
    0,   27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,  '*', 0, ' ',
};

/* keymap ketika shift ditekan */
static const char keymap_shift[128] = {
    0,   27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,  '*', 0, ' ',
};

static uint8_t keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile int read_pos = 0;
static volatile int write_pos = 0;
static volatile uint8_t ctrl_pressed = 0;
static volatile uint8_t extended_key = 0;
static volatile uint8_t shift_pressed = 0;

/* cek buffer penuh apa gak */
static int buffer_is_full(void) {
    int next = (write_pos + 1) % KEYBOARD_BUFFER_SIZE;
    return next == read_pos;
}

/* inisialisasi keyboard */
void keyboard_init(void) {
    /* buang byte yang masih ngepend di controller keyboard */
    (void)inb(KEYBOARD_DATA);
    ctrl_pressed = 0;
    extended_key = 0;
}

/* handler interrupt keyboard */
void keyboard_interrupt_handler(void) {
    uint8_t scancode = inb(KEYBOARD_DATA);

    /* handle prefix extended key */
    if (scancode == 0xE0) {
        extended_key = 1;
        return;
    }

    /* handle extended keys (arrow keys, dll) */
    if (extended_key) {
        extended_key = 0;

        /* cuekin event lepas tombol */
        if (scancode & 0x80) {
            return;
        }

        uint8_t key = KEY_NONE;
        switch (scancode) {
            case 0x48: key = KEY_ARROW_UP; break;
            case 0x50: key = KEY_ARROW_DOWN; break;
            case 0x4B: key = KEY_ARROW_LEFT; break;
            case 0x4D: key = KEY_ARROW_RIGHT; break;
            case 0x53: key = KEY_DELETE; break;
            case 0x47: key = KEY_HOME; break;
            case 0x4F: key = KEY_END; break;
            case 0x49: key = KEY_PGUP; break;
            case 0x51: key = KEY_PGDN; break;
        }

        if (key != KEY_NONE && !buffer_is_full()) {
            keyboard_buffer[write_pos] = key;
            write_pos = (write_pos + 1) % KEYBOARD_BUFFER_SIZE;
        }
        return;
    }

    /* handle shift keys */
    if (scancode == 0x2A) {          // left shift press
        shift_pressed = 1;
        return;
    }
    if (scancode == 0xAA) {          // left shift release
        shift_pressed = 0;
        return;
    }
    if (scancode == 0x36) {          // right shift press
        shift_pressed = 1;
        return;
    }
    if (scancode == 0xB6) {          // right shift release
        shift_pressed = 0;
        return;
    }

    /* handle tombol ctrl */
    if (scancode == 0x1D) {
        ctrl_pressed = 1;
        return;
    }
    if (scancode == 0x9D) {
        ctrl_pressed = 0;
        return;
    }

    /* cuekin event lepas tombol */
    if (scancode & 0x80) {
        return;
    }

    /* jaga-jaga kalo scancode di luar range */
    if (scancode >= 128) {
        return;
    }

    /* handle kombinasi ctrl+key */
    if (ctrl_pressed) {
        char c = keymap_us[scancode];
        uint8_t key = KEY_NONE;

        if (c == 's') key = KEY_CTRL_S;
        else if (c == 'x') key = KEY_CTRL_X;
        else if (c == 'z') key = KEY_CTRL_Z;

        if (key != KEY_NONE && !buffer_is_full()) {
            keyboard_buffer[write_pos] = key;
            write_pos = (write_pos + 1) % KEYBOARD_BUFFER_SIZE;
        }
        return;
    }

    const char *map = shift_pressed ? keymap_shift : keymap_us;
    char c = map[scancode];

    if (c != 0) {
        if (!buffer_is_full()) {
            keyboard_buffer[write_pos] = (uint8_t)c;
            write_pos = (write_pos + 1) % KEYBOARD_BUFFER_SIZE;
        }
    }
}

/* buka kunci keyboard controller */
void keyboard_flush(void) {
    read_pos = 0;
    write_pos = 0;
    ctrl_pressed = 0;
    extended_key = 0;
    /* buang byte pending */
    for (int i = 0; i < 32; i++) (void)inb(KEYBOARD_DATA);
}

/* ambil satu karakter dari buffer keyboard (hanya printable chars) */
int keyboard_available(void) {
    return (read_pos != write_pos);
}

char keyboard_getchar(void) {
    while (1) {
        while (read_pos == write_pos) {
            /* sti;hlt itu atomik di x86: interrupt nyala pas hlt, bukan antara sti sama hlt */
            asm volatile("sti; hlt");
        }

        uint8_t key = keyboard_buffer[read_pos];

        /* skip special keys, cuma return printable chars */
        if (key < 0x80) {
            read_pos = (read_pos + 1) % KEYBOARD_BUFFER_SIZE;
            return (char)key;
        }

        /* buang special key dan lanjut nunggu */
        read_pos = (read_pos + 1) % KEYBOARD_BUFFER_SIZE;
    }
}

/* ambil satu key dari buffer (termasuk special keys) */
uint8_t keyboard_getkey(void) {
    while (read_pos == write_pos) {
        asm volatile("sti; hlt");
    }

    uint8_t key = keyboard_buffer[read_pos];
    read_pos = (read_pos + 1) % KEYBOARD_BUFFER_SIZE;
    return key;
}
