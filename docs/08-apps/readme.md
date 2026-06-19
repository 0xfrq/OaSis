# aplikasi

dokumentasi ini ngebahas aplikasi yang jalan di OaSis.

## daftar isi

- [overview](#overview)
- [text editor](#text-editor)
- [simple assembler](#simple-assembler)
- [cara bikin aplikasi baru](#cara-bikin-aplikasi-baru)
- [integrasi dengan kernel](#integrasi-dengan-kernel)

---

## overview

**aplikasi** di OaSis adalah program yang jalan di atas kernel dan provide functionality buat user.

### karakteristik aplikasi

- jalan di kernel mode (belum ada user mode)
- pake kernel API (vfs, vga, keyboard, dll)
- di-invoke dari shell
- punya entry point function

### aplikasi yang available

- **text editor**: editor sederhana buat edit file
- **simple assembler**: assembler x86 interaktif buat nulis & jalanin kode assembly langsung di shell

## text editor

text editor adalah aplikasi pertama yang dibuat buat OaSis. mirip sama nano/vim tapi versi sederhana.

### fitur

- **buka file**: load file dari filesystem
- **edit text**: tambah, hapus, modify text
- **navigasi**: arrow keys buat move cursor
- **save**: simpan perubahan ke file (ctrl+s)
- **exit**: keluar dari editor (ctrl+x)
- **scrolling**: automatic scroll kalo file panjang
- **status bar**: tampilkan info (filename, line, column)

### cara pake

```bash
edit <path>
```

**contoh:**
```
> edit /document.txt
```

### kontrol

| key | fungsi |
|-----|--------|
| arrow keys | move cursor |
| backspace | hapus karakter sebelum cursor |
| delete | hapus karakter di cursor |
| enter | insert newline |
| ctrl+s | save file |
| ctrl+x | exit (auto-save if modified) |
| home | ke awal baris |
| end | ke ujung baris |

### interface

```
┌────────────────────────────────────────┐
│                                        │
│  text content here                     │
│  line 2                                │
│  line 3                                │
│  ...                                   │
│                                        │
├────────────────────────────────────────┤
│ /document.txt  [*]  Ln 5, Col 12       │  status bar
├────────────────────────────────────────┤
│ ^S save  ^X exit                       │  help bar
└────────────────────────────────────────┘
```

### implementasi

**file:** `src/apps/editor.c`

**struktur:**
```c
void editor_run(const char *path) {
    // 1. load file
    editor_load(path);
    
    // 2. main loop
    while (running) {
        // draw screen
        editor_draw();
        
        // handle input
        uint8_t key = keyboard_getkey();
        editor_handle_key(key);
    }
    
    // 3. cleanup
    vga_clear();
}
```

**text buffer:**
```c
#define EDITOR_BUFFER_SIZE 4096

static char text_buf[EDITOR_BUFFER_SIZE];
static uint32_t buf_used = 0;
static uint32_t cursor_pos = 0;
```

**scrolling:**
```c
// track visible area
static int scroll_row = 0;

// update scroll to keep cursor visible
void update_scroll(void) {
    int line = get_cursor_line();
    
    if (line < scroll_row) {
        scroll_row = line;
    }
    
    if (line >= scroll_row + EDITOR_TEXT_ROWS) {
        scroll_row = line - EDITOR_TEXT_ROWS + 1;
    }
}
```

detail ada di [editor.md](editor.md)

## simple assembler

simple assembler adalah aplikasi kedua di OaSis. user bisa nulis kode assembly x86 baris per baris, lalu assembler bakal compile jadi machine code dan langsung dijalanin.

### fitur

- **interactive mode**: tulis kode baris per baris, akhiri dengan `---`
- **instruksi x86 32-bit**:
  - arithmetic: `mov`, `add`, `sub`, `cmp`, `xor`, `and`, `or`, `inc`, `dec`
  - stack: `push`, `pop`, `pusha`, `popa`
  - control flow: `jmp`, `je/jz`, `jne/jnz`, `jg`, `jl`, `jge`, `jle`, `call`, `ret`
  - system: `int imm8`, `nop`, `hlt`, `sti`, `cli`
  - data: `db 'string'`, `db 0x41`
- **register**: `eax`, `ecx`, `edx`, `ebx`, `esp`, `ebp`, `esi`, `edi`
- **label**: diakhiri `:`, support forward & backward reference, juga inline (`label: instr`)
- **komentar**: diawali `;`
- **memory addressing sederhana**: `[0xB8000]`, `[eax]`, `mov byte [addr], imm8`
- **immediate**: desimal atau hex (`0x...`)
- **auto-ret**: sebelum data block dan di akhir kode, assembler otomatis sisip `ret` biar setelah eksekusi balik ke shell

### cara pake

ada dua mode:

**1. mode interaktif** - tulis kode langsung di shell:

```bash
asm
```

masuk mode interaktif, tulis kode baris per baris, akhiri dengan `---`.

**2. mode file** - simpan dulu ke file, lalu assemble:

```bash
edit hello.asm     # tulis kode, Ctrl+S simpan, Ctrl+X keluar
nasm hello.asm     # assemble & jalanin
```

workflow umum:
1. `edit <file>.asm` - tulis kode pake editor nano-like
2. `cat <file>.asm` - cek isi file (opsional)
3. `nasm <file>.asm` - assemble & jalanin, output bakal nunjukin: jumlah byte machine code, alamat virtual, hex dump, lalu output dari eksekusi
4. setelah selesai, otomatis balik ke shell

### contoh hello world

```asm
mov eax, 0          ; sys_write
mov ebx, msg        ; pointer string
mov ecx, 13         ; panjang string
int 0x80
msg:
db 'Hello World!'
---
```

setelah eksekusi:

```
Mengassemble kode...
Kode mesin: 14 byte di alamat 0x200000
Bytes: b8 00 00 00 00 bb ... cd 80 c3 48 65 6c 6c ...
Menjalankan...
Hello World!
[selesai]
```

### output assembler

setiap kali user selesai input dengan `---`, assembler nge-print:

1. jumlah byte machine code yang di-generate
2. alamat virtual tempat kode di-load
3. hex dump dari beberapa byte awal
4. eksekusi kode, lalu return ke shell

### implementasi

**file:** `src/kernel/apps/asm.c`

**alur kerja:**

```
1. user input baris per baris
   ↓
2. parser baca tiap baris (skip komentar, label, dll)
   ↓
3. generator emit machine code ke buffer statis
   ↓
4. label di-resolve, forward jumps di-patch
   ↓
5. alokasi halaman fisik via pmm_alloc_page()
   ↓
6. map ke virtual address 0x200000 via page_map()
   ↓
7. salin machine code ke virtual address
   ↓
8. invlpg buat flush TLB
   ↓
9. cast pointer ke fungsi, lalu panggil
   ↓
10. setelah ret, balik ke asm_run, lalu ke shell
```

**struktur utama:**

```c
#define CODE_VIRT  0x00200000   /* alamat virtual buat kode */
#define CODE_SIZE  4096

static uint8_t code_buf[CODE_SIZE];   /* buffer machine code */
static int code_len;

/* tabel label */
static struct {
    char name[32];
    int  pos;
} labels[MAX_LABELS];

/* patch buat forward reference */
static struct {
    int pos, from;
    char target[32];
    int type;  /* 0=rel8, 1=rel32, 2=abs32 */
} patches[MAX_PATCHES];
```

**parser instruksi:**

```c
static int process_line(char *line) {
    /* skip komentar, parse label */
    /* split mnemonic dan operand */
    /* dispatch ke generator (gen_mov, gen_add, gen_jmp, dll) */
}
```

**code generator (contoh mov):**

```c
static int gen_mov(char *ops) {
    /* mov r32, r32:    89 /r */
    /* mov r32, imm32:  B8+rd id */
    /* mov r32, [mem]:  8B /r */
    /* mov [mem], r32:  89 /r */
    /* mov byte [m], imm8: C6 /0 ib */
}
```

**auto-return:**

assembler otomatis sisip `ret` (0xC3) di dua tempat:
1. sebelum baris `db ...` (kalau byte terakhir bukan terminator)
2. di akhir semua kode (kalau byte terakhir bukan ret/jmp/hlt)

ini bikin kode user otomatis balik ke shell setelah selesai, tanpa harus nulis `ret` manual.

### syscall yang bisa dipake

assembler bisa panggil syscall OaSis via `int 0x80`:

| eax | syscall | argumen (ebx, ecx, edx) |
|-----|---------|--------------------------|
| 0 | write | (string ptr, length) |
| 1 | sleep | (milliseconds) |
| 2 | yield | - |
| 3 | exit | (code) |
| 4 | getpid | - |
| 11 | read | (fd, buf, count) |
| 12 | write_fd | (fd, buf, count) |

lihat `include/syscall.h` buat daftar lengkap.

## cara bikin aplikasi baru

### step 1: buat file

bikin file baru di `src/apps/`:

```c
// src/apps/myapp.c
#include "vga.h"
#include "keyboard.h"
#include "vfs.h"

void myapp_run(const char *args) {
    vga_puts("my application\n");
    
    // do something
    
    vga_puts("done!\n");
}
```

### step 2: bikin header

```c
// include/myapp.h
#ifndef MYAPP_H
#define MYAPP_H

void myapp_run(const char *args);

#endif
```

### step 3: integrate ke shell

edit `src/kernel/shell.c`:

```c
#include "myapp.h"

void shell_execute(const char *cmd, const char *args) {
    // ... existing commands ...
    
    else if (strcmp(cmd, "myapp") == 0) {
        myapp_run(args);
    }
}
```

### step 4: update makefile

edit `Makefile`:

```makefile
SOURCES_APPS = src/apps/editor.c \
               src/apps/myapp.c
```

### step 5: build dan test

```bash
make clean && make
make run
```

test di shell:
```
> myapp
my application
done!
```

## integrasi dengan kernel

aplikasi bisa pake semua kernel API:

### vga (display)

```c
#include "vga.h"

vga_puts("hello\n");
vga_putchar('A');
vga_set_color(VGA_COLOR_RED, VGA_COLOR_BLACK);
```

### keyboard (input)

```c
#include "keyboard.h"

char c = keyboard_getchar();  // blocking
uint8_t key = keyboard_getkey();  // includes special keys
```

### vfs (filesystem)

```c
#include "vfs.h"

int fd = vfs_open("/file.txt", VFS_O_READ);
char buf[100];
int n = vfs_read(fd, buf, sizeof(buf));
vfs_close(fd);
```

### string (utility)

```c
#include "string.h"

int len = strlen(str);
memcpy(dest, src, n);
int cmp = strcmp(str1, str2);
```

### contoh: file viewer

```c
// src/apps/viewer.c
#include "vga.h"
#include "keyboard.h"
#include "vfs.h"
#include "string.h"

void viewer_run(const char *path) {
    // open file
    int fd = vfs_open(path, VFS_O_READ);
    if (fd < 0) {
        vga_puts("error: cannot open file\n");
        return;
    }
    
    // read and display
    char buf[1024];
    int n = vfs_read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        vga_puts(buf);
    }
    
    vfs_close(fd);
    
    // wait for keypress
    vga_puts("\n[press any key to continue]\n");
    keyboard_getchar();
}
```

---

## future applications

ide aplikasi yang bisa ditambah:

- **file manager**: navigate dan manage files
- **hex editor**: edit binary files
- **calculator**: simple calculator
- **game**: simple text-based game
- **system monitor**: show system info (memory, tasks, dll)
- **c compiler**: jangka panjang, biar bisa coding c langsung di OaSis (saat ini baru bisa assembly)

---

**kembali ke:** [dokumentasi →](../readme.md) | **selanjutnya:** [build system →](../09-build/readme.md)
