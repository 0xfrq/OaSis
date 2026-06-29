# vga driver

dokumentasi ini membahas bagaimana OaSis menampilkan teks ke layar.

## daftar isi

- [overview](#overview)
- [vga text mode](#vga-text-mode)
- [memory layout](#memory-layout)
- [warna](#warna)
- [cursor](#cursor)
- [api reference](#api-reference)
- [contoh penggunaan](#contoh-penggunaan)

---

## overview

**vga (video graphics array)** driver di OaSis handle output ke layar menggunakan text mode.

### kenapa text mode?

- simpler dari graphics mode
- cukup untuk command line interface
- standard di x86
- mudah di-debug

### kemampuan

- 80 kolom x 25 baris
- 16 warna foreground
- 16 warna background
- hardware cursor

## vga text mode

vga text mode adalah mode dimana layar dibagi jadi grid karakter.

### grid

```
┌────────────────────────────────────────┐
│ (0,0)                              (79,0) │
│                                        │
│                                        │
│                                        │
│                                        │
│ (0,24)                            (79,24)│
└────────────────────────────────────────┘
```

- 80 kolom (x: 0-79)
- 25 baris (y: 0-24)
- total: 2000 karakter

## memory layout

vga buffer ada di memory address `0xB8000`.

### struktur

setiap karakter direpresentasikan sama 2 bytes:

```
offset 0: character (ascii)
offset 1: attribute (color)
```

**contoh:**
```
address: 0xB8000  0xB8001  0xB8002  0xB8003
data:    'H'      0x0F     'i'      0x0F
         char     color    char     color
```

### perhitungan offset

```c
offset = (y * 80 + x) * 2
```

**contoh:** karakter di (10, 5):
```
offset = (5 * 80 + 10) * 2
       = 410 * 2
       = 820
```

### implementasi

```c
#define VGA_MEMORY 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25

uint16_t *vga_buffer = (uint16_t *)VGA_MEMORY;

void vga_putchar(char c, uint8_t color, uint8_t x, uint8_t y) {
    uint16_t entry = c | (color << 8);
    vga_buffer[y * VGA_WIDTH + x] = entry;
}
```

## warna

vga support 16 warna:

| kode | warna |
|------|-------|
| 0 | black |
| 1 | blue |
| 2 | green |
| 3 | cyan |
| 4 | red |
| 5 | magenta |
| 6 | brown |
| 7 | light gray |
| 8 | dark gray |
| 9 | light blue |
| 10 | light green |
| 11 | light cyan |
| 12 | light red |
| 13 | light magenta |
| 14 | yellow |
| 15 | white |

### attribute byte

attribute byte format:

```
bit: 7 6 5 4  3 2 1 0
     └─┬─┘    └─┬─┘
       │        │
       │        └─ foreground color (0-15)
       │
       └─ background color (0-15)
```

**contoh:**
- `0x0F` = white text on black background
- `0x4F` = white text on red background
- `0x1E` = yellow text on blue background

### kombinasi warna

```c
uint8_t color = (bg << 4) | fg;

// white on black
uint8_t white_black = (0 << 4) | 15;  // 0x0F

// yellow on blue
uint8_t yellow_blue = (1 << 4) | 14;  // 0x1E
```

## cursor

vga memiliki hardware cursor yang bisa di-enable/disable dan dipindahkan.

### set cursor position

```c
void vga_set_cursor(uint8_t x, uint8_t y) {
    uint16_t pos = y * VGA_WIDTH + x;
    
    // send position to vga controller
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}
```

### enable cursor

```c
void vga_enable_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 0);  // cursor start line
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15); // cursor end line
}
```

### disable cursor

```c
void vga_disable_cursor(void) {
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);  // disable cursor
}
```

## api reference

### inisialisasi

```c
void vga_init(void);
```

inisialisasi vga driver. clear screen dan setup cursor.

### clear screen

```c
void vga_clear(void);
```

clear seluruh layar (isi dengan spasi).

**contoh:**
```c
vga_clear();
```

### tulis karakter

```c
void vga_putchar(char c);
```

tulis satu karakter di posisi cursor sekarang.

**parameter:**
- `c`: karakter yang mau ditulis

**contoh:**
```c
vga_putchar('A');
```

### tulis string

```c
void vga_puts(const char *str);
```

tulis string di posisi cursor sekarang.

**parameter:**
- `str`: string yang mau ditulis (null-terminated)

**contoh:**
```c
vga_puts("hello world\n");
```

### set warna

```c
void vga_set_color(uint8_t fg, uint8_t bg);
```

set warna untuk operasi tulis berikutnya.

**parameter:**
- `fg`: foreground color (0-15)
- `bg`: background color (0-15)

**contoh:**
```c
vga_set_color(14, 1);  // yellow text on blue background
vga_puts("warning!");
```

### set cursor position

```c
void vga_set_cursor(uint8_t x, uint8_t y);
```

memindahkan cursor ke posisi tertentu.

**parameter:**
- `x`: kolom (0-79)
- `y`: baris (0-24)

**contoh:**
```c
vga_set_cursor(0, 0);  // pojok kiri atas
```

### get cursor position

```c
uint8_t vga_get_cursor_x(void);
uint8_t vga_get_cursor_y(void);
```

mendapatkan posisi cursor sekarang.

**return:** posisi x atau y

### scroll

```c
void vga_scroll(void);
```

scroll layar ke atas 1 baris. baris paling bawah jadi kosong.

**catatan:** biasanya dipanggil otomatis saat cursor sampai baris paling bawah.

## contoh penggunaan

### contoh 1: hello world

```c
void main(void) {
    vga_init();
    vga_puts("hello, world!\n");
}
```

### contoh 2: warna

```c
void main(void) {
    vga_init();
    
    vga_set_color(15, 0);  // white on black
    vga_puts("normal text\n");
    
    vga_set_color(4, 0);   // red on black
    vga_puts("error!\n");
    
    vga_set_color(2, 0);   // green on black
    vga_puts("success!\n");
}
```

### contoh 3: cursor positioning

```c
void main(void) {
    vga_init();
    vga_clear();
    
    // tulis di pojok kiri atas
    vga_set_cursor(0, 0);
    vga_puts("top-left");
    
    // tulis di tengah
    vga_set_cursor(36, 12);
    vga_puts("center");
    
    // tulis di pojok kanan bawah
    vga_set_cursor(70, 24);
    vga_puts("bottom-right");
}
```

### contoh 4: status bar

```c
void draw_status_bar(void) {
    // save current color
    uint8_t old_color = current_color;
    
    // draw bar (white on blue)
    vga_set_color(15, 1);
    vga_set_cursor(0, 24);
    vga_puts("status: ready  |  press ctrl+c to exit");
    
    // restore color
    current_color = old_color;
}
```

---

## troubleshooting

### layar blank

- cek vga buffer address (harus 0xB8000)
- cek inisialisasi vga
- cek qemu parameter (harus menggunakan vga mode)

### karakter aneh

- cek attribute byte (warna)
- cek ascii code karakter
- cek offset calculation

### cursor tidak muncul

- cek cursor enable
- cek cursor position (harus dalam range 0-79, 0-24)
- cek vga controller ports

---

**kembali ke:** [driver →](readme.md)
