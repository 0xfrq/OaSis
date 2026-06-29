# keyboard driver

dokumentasi ini membahas bagaimana OaSis baca input dari keyboard.

## daftar isi

- [overview](#overview)
- [keyboard ps/2](#keyboard-ps2)
- [scancode](#scancode)
- [buffer](#buffer)
- [interrupt handling](#interrupt-handling)
- [api reference](#api-reference)
- [contoh penggunaan](#contoh-penggunaan)

---

## overview

**keyboard driver** di OaSis handle input dari keyboard PS/2.

### kemampuan

- baca scancode dari keyboard
- convert scancode ke ASCII pakai layout US QWERTY
- support modifier keys (**shift** kiri & kanan, **ctrl**)
- support **shifted symbols** (`!@#$%^&*()_+{}:"<>?~|` dst)
- support **arrow keys**, Home, End, PageUp/Down, Delete (via extended scancode 0xE0)
- buffering (circular queue 256 byte)
- interrupt-driven

### limitation

- belum mendukung USB keyboard
- belum mendukung function keys (F1-F12)
- belum mendukung Caps Lock
- belum mendukung keyboard LED
- belum mendukung Alt key

## keyboard ps2

keyboard PS/2 communicate lewat port I/O:

- **0x60**: data port (baca scancode)
- **0x64**: status port (cek data available)

### status register

```
bit 0: output buffer full (data ready to read)
bit 1: input buffer full
bit 2-7: other flags
```

### baca scancode

```c
uint8_t scancode = inb(0x60);
```

## scancode

**scancode** adalah kode yang dikirim keyboard saat tombol ditekan/dilepas.

### make code vs break code

- **make code**: tombol ditekan
- **break code**: tombol dilepas (make code + 0x80)

**contoh:**
- tekan 'A': scancode 0x1E
- lepas 'A': scancode 0x9E (0x1E + 0x80)

### scancode table (set 1)

| key | scancode | key | scancode |
|-----|----------|-----|----------|
| ESC | 0x01 | A | 0x1E |
| 1 | 0x02 | S | 0x1F |
| 2 | 0x03 | D | 0x20 |
| 3 | 0x04 | F | 0x21 |
| 4 | 0x05 | G | 0x22 |
| 5 | 0x06 | H | 0x23 |
| 6 | 0x07 | J | 0x24 |
| 7 | 0x08 | K | 0x25 |
| 8 | 0x09 | L | 0x26 |
| 9 | 0x0A | ; | 0x27 |
| 0 | 0x0B | ' | 0x28 |
| - | 0x0C | ` | 0x29 |
| = | 0x0D | \ | 0x2B |
| BACKSPACE | 0x0E | ENTER | 0x1C |
| TAB | 0x0F | SPACE | 0x39 |
| Q | 0x10 | LSHIFT | 0x2A |
| W | 0x11 | RSHIFT | 0x36 |
| E | 0x12 | LCTRL | 0x1D |
| R | 0x13 | CAPSLOCK | 0x3A |
| T | 0x14 | F1 | 0x3B |
| Y | 0x15 | F2 | 0x3C |
| U | 0x16 | F3 | 0x3D |
| I | 0x17 | F4 | 0x3E |
| O | 0x18 | F5 | 0x3F |
| P | 0x19 | F6 | 0x40 |
| [ | 0x1A | F7 | 0x41 |
| ] | 0x1B | F8 | 0x42 |

### konversi ke ascii

OaSis pakai **dua keymap**: satu untuk kondisi tanpa shift, satu lagi saat shift ditekan.

```c
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

/* di handler, pilih keymap berdasarkan status shift */
const char *map = shift_pressed ? keymap_shift : keymap_us;
char c = map[scancode];
```

### tracking modifier

```c
static volatile uint8_t shift_pressed = 0;
static volatile uint8_t ctrl_pressed = 0;
static volatile uint8_t extended_key = 0;

/* shift kiri: 0x2A press, 0xAA release */
/* shift kanan: 0x36 press, 0xB6 release */
/* ctrl: 0x1D press, 0x9D release */
/* extended (arrow keys dan lain-lain): prefix 0xE0 */
```

## buffer

keyboard driver pakai **circular buffer** untuk menyimpan karakter yang belum dibaca.

### kenapa butuh buffer?

- keyboard interrupt bisa terjadi kapan saja
- aplikasi mungkin belum siap baca
- buffer menyimpan karakter sampai aplikasi baca

### struktur buffer

```c
#define KEYBOARD_BUFFER_SIZE 256

char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
uint8_t keyboard_buffer_head = 0;
uint8_t keyboard_buffer_tail = 0;
```

### operasi buffer

**tambah karakter:**
```c
void keyboard_buffer_put(char c) {
    uint8_t next = (keyboard_buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
    if (next != keyboard_buffer_tail) {
        keyboard_buffer[keyboard_buffer_head] = c;
        keyboard_buffer_head = next;
    }
}
```

**baca karakter:**
```c
char keyboard_buffer_get(void) {
    if (keyboard_buffer_head == keyboard_buffer_tail) {
        return 0;  // buffer empty
    }
    char c = keyboard_buffer[keyboard_buffer_tail];
    keyboard_buffer_tail = (keyboard_buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}
```

## interrupt handling

keyboard pakai **IRQ 1** (interrupt 33 setelah remapping).

### flow

```
user press key
  ↓
keyboard controller generate IRQ 1
  ↓
pic forward ke cpu sebagai int 33
  ↓
cpu lompat ke isr 33
  ↓
keyboard_handler() dipanggil
  ↓
baca scancode dari port 0x60
  ↓
convert ke ascii
  ↓
store di buffer
  ↓
send EOI ke pic
  ↓
return dari interrupt
```

### implementasi

```c
void keyboard_handler(void) {
    uint8_t scancode = inb(0x60);
    
    // ignore break codes (key release)
    if (scancode & 0x80) {
        goto done;
    }
    
    // convert to ascii
    char c = scancode_to_ascii(scancode);
    
    // store in buffer
    if (c != 0) {
        keyboard_buffer_put(c);
    }
    
done:
    // send EOI to PIC
    pic_send_eoi(1);  // IRQ 1
}
```

## api reference

### inisialisasi

```c
void keyboard_init(void);
```

inisialisasi keyboard driver. register interrupt handler.

**yang dilakukan:**
1. register keyboard_handler() untuk IRQ 1
2. enable IRQ 1 di PIC
3. clear buffer

### baca karakter

```c
char keyboard_getchar(void);
```

baca satu karakter dari buffer. **blocking** (tunggu sampai ada karakter).

**return:** karakter yang dibaca

**contoh:**
```c
char c = keyboard_getchar();
vga_putchar(c);
```

### cek karakter available

```c
int keyboard_available(void);
```

cek apakah ada karakter di buffer. **non-blocking**.

**return:**
- `1`: ada karakter
- `0`: buffer kosong

**contoh:**
```c
if (keyboard_available()) {
    char c = keyboard_getchar();
    // process character
}
```

### baca string

```c
int keyboard_gets(char *buf, int max_len);
```

baca string sampai newline atau max_len. **blocking**.

**parameter:**
- `buf`: buffer untuk menyimpan string
- `max_len`: maximum length (termasuk null terminator)

**return:** jumlah karakter yang dibaca

**contoh:**
```c
char input[100];
keyboard_gets(input, sizeof(input));
```

## contoh penggunaan

### contoh 1: echo

```c
void main(void) {
    vga_init();
    keyboard_init();
    
    vga_puts("type something:\n");
    
    while (1) {
        char c = keyboard_getchar();
        vga_putchar(c);
        
        if (c == '\n') {
            break;
        }
    }
}
```

### contoh 2: simple shell

```c
void simple_shell(void) {
    char input[100];
    
    while (1) {
        vga_puts("> ");
        keyboard_gets(input, sizeof(input));
        
        if (strcmp(input, "help") == 0) {
            vga_puts("available commands: help, clear, exit\n");
        } else if (strcmp(input, "clear") == 0) {
            vga_clear();
        } else if (strcmp(input, "exit") == 0) {
            break;
        } else {
            vga_puts("unknown command\n");
        }
    }
}
```

### contoh 3: non-blocking input

```c
void game_loop(void) {
    while (1) {
        // update game state
        update_game();
        
        // check for input (non-blocking)
        if (keyboard_available()) {
            char c = keyboard_getchar();
            handle_input(c);
        }
        
        // render
        render_game();
    }
}
```

---

## troubleshooting

### keyboard tidak response

- cek IRQ 1 enabled di PIC
- cek keyboard_handler() registered di IDT
- cek port 0x60 (harus bisa dibaca)

### karakter aneh

- cek scancode table
- cek modifier keys (shift, capslock)
- cek make/break code handling

### buffer overflow

- cek buffer size (currently 256)
- cek aplikasi baca buffer regularly
- cek circular buffer logic

---

**kembali ke:** [driver →](readme.md)
