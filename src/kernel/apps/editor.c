#include "editor.h"
#include "vga.h"
#include "keyboard.h"
#include "vfs.h"
#include "string.h"
#include <stdint.h>

/* buffer teks */
static char text_buf[EDITOR_BUFFER_SIZE];
static uint32_t buf_used = 0;

/* state editor */
static uint32_t cursor_pos = 0;
static int scroll_row = 0;
static int modified = 0;
static char file_path[MAX_PATH_LENGTH];

/* skema warna */
#define COLOR_TEXT (VGA_COLOR_WHITE | (VGA_COLOR_BLACK << 4))
#define COLOR_STATUS (VGA_COLOR_BLACK | (VGA_COLOR_YELLOW << 4))
#define COLOR_HELP (VGA_COLOR_BLACK | (VGA_COLOR_LIGHT_GREEN << 4))

/* cari posisi awal dari baris tertentu */
static uint32_t find_line_start(int line_num) {
    uint32_t pos = 0;
    int current_line = 0;

    while (pos < buf_used && current_line < line_num) {
        if (text_buf[pos] == '\n') {
            current_line++;
        }
        pos++;
    }
    return pos;
}

/* dapetin nomor baris sekarang dari posisi cursor */
static int get_cursor_line(void) {
    int line = 0;
    for (uint32_t i = 0; i < cursor_pos && i < buf_used; i++) {
        if (text_buf[i] == '\n') {
            line++;
        }
    }
    return line;
}

/* dapetin kolom sekarang dari posisi cursor */
static int get_cursor_col(void) {
    uint32_t pos = cursor_pos;
    while (pos > 0 && text_buf[pos - 1] != '\n') {
        pos--;
    }
    return cursor_pos - pos;
}

/* dapetin panjang baris sekarang (tanpa newline) */
static int get_current_line_length(void) {
    int line = get_cursor_line();
    uint32_t start = find_line_start(line);
    uint32_t pos = start;

    while (pos < buf_used && text_buf[pos] != '\n') {
        pos++;
    }
    return pos - start;
}

/* masukin karakter di posisi cursor */
static void insert_char(char c) {
    if (buf_used >= EDITOR_BUFFER_SIZE - 1) {
        return; /* buffer penuh */
    }

    /* geser semua yang setelah cursor ke kanan */
    memmove(&text_buf[cursor_pos + 1], &text_buf[cursor_pos], buf_used - cursor_pos);
    text_buf[cursor_pos] = c;
    buf_used++;
    cursor_pos++;
    modified = 1;
}

/* hapus karakter di posisi cursor */
static void delete_char(void) {
    if (cursor_pos >= buf_used) {
        return;
    }

    /* geser semua yang setelah cursor ke kiri */
    memmove(&text_buf[cursor_pos], &text_buf[cursor_pos + 1], buf_used - cursor_pos - 1);
    buf_used--;
    modified = 1;
}

/* backspace: hapus karakter sebelum cursor */
static void backspace(void) {
    if (cursor_pos == 0) {
        return;
    }

    cursor_pos--;
    delete_char();
}

/* masukin newline di cursor */
static void insert_newline(void) {
    insert_char('\n');
}

/* geser cursor ke atas */
static void move_cursor_up(void) {
    int line = get_cursor_line();
    if (line == 0) {
        return;
    }

    int col = get_cursor_col();
    uint32_t prev_line_start = find_line_start(line - 1);
    uint32_t prev_line_end = find_line_start(line) - 1; /* -1 buat \n */

    /* pindah ke kolom yang sama di baris sebelumnya, atau ke ujung kalo lebih pendek */
    uint32_t new_pos = prev_line_start + col;
    if (new_pos > prev_line_end) {
        new_pos = prev_line_end;
    }
    cursor_pos = new_pos;
}

/* geser cursor ke bawah */
static void move_cursor_down(void) {
    int line = get_cursor_line();
    uint32_t next_line_start = find_line_start(line + 1);

    if (next_line_start >= buf_used) {
        return; /* udah di baris terakhir */
    }

    int col = get_cursor_col();
    uint32_t next_line_end = next_line_start;

    /* cari ujung baris berikutnya */
    while (next_line_end < buf_used && text_buf[next_line_end] != '\n') {
        next_line_end++;
    }

    /* pindah ke kolom yang sama di baris berikutnya, atau ke ujung kalo lebih pendek */
    uint32_t new_pos = next_line_start + col;
    if (new_pos > next_line_end) {
        new_pos = next_line_end;
    }
    cursor_pos = new_pos;
}

/* geser cursor ke kiri */
static void move_cursor_left(void) {
    if (cursor_pos > 0) {
        cursor_pos--;
    }
}

/* geser cursor ke kanan */
static void move_cursor_right(void) {
    if (cursor_pos < buf_used) {
        cursor_pos++;
    }
}

/* geser cursor ke awal baris */
static void move_cursor_home(void) {
    int line = get_cursor_line();
    cursor_pos = find_line_start(line);
}

/* geser cursor ke ujung baris */
static void move_cursor_end(void) {
    int line = get_cursor_line();
    uint32_t line_start = find_line_start(line);
    uint32_t pos = line_start;

    while (pos < buf_used && text_buf[pos] != '\n') {
        pos++;
    }
    cursor_pos = pos;
}

/* gambar area teks (baris 0-22) */
static void draw_text_area(void) {
    /* bersihin area teks */
    vga_fill_rect(0, 0, EDITOR_SCREEN_WIDTH, EDITOR_TEXT_ROWS, ' ', COLOR_TEXT);

    /* gambar baris yang keliatan */
    int line = scroll_row;
    for (int row = 0; row < EDITOR_TEXT_ROWS && find_line_start(line) <= buf_used; row++) {
        uint32_t line_start = find_line_start(line);
        uint32_t pos = line_start;

        /* gambar karakter di baris ini */
        int col = 0;
        while (pos < buf_used && text_buf[pos] != '\n' && col < EDITOR_SCREEN_WIDTH) {
            vga_write_char(col, row, text_buf[pos], COLOR_TEXT);
            pos++;
            col++;
        }
        line++;
    }
}

/* gambar status bar (baris 23) */
static void draw_status_bar(void) {
    char status[EDITOR_SCREEN_WIDTH + 1];
    memset(status, ' ', EDITOR_SCREEN_WIDTH);
    status[EDITOR_SCREEN_WIDTH] = '\0';

    /* nama file */
    int pos = 0;
    const char *fname = file_path;
    while (*fname && pos < 30) {
        status[pos++] = *fname++;
    }

    /* indikator modified */
    if (modified) {
        status[pos++] = ' ';
        status[pos++] = '[';
        status[pos++] = '*';
        status[pos++] = ']';
    }

    /* baris dan kolom */
    char line_col[20];
    int line = get_cursor_line() + 1;
    int col = get_cursor_col() + 1;
    itoa(line, line_col, 10);
    int len = strlen(line_col);
    status[pos++] = ' ';
    memcpy(&status[pos], "Br ", 3);
    pos += 3;
    memcpy(&status[pos], line_col, len);
    pos += len;

    itoa(col, line_col, 10);
    len = strlen(line_col);
    status[pos++] = ',';
    status[pos++] = ' ';
    memcpy(&status[pos], "Kol ", 4);
    pos += 4;
    memcpy(&status[pos], line_col, len);
    pos += len;

    /* gambar status bar */
    for (int i = 0; i < EDITOR_SCREEN_WIDTH; i++) {
        vga_write_char(i, EDITOR_STATUS_ROW, status[i], COLOR_STATUS);
    }
}

/* gambar help bar (baris 24) */
static void draw_help_bar(void) {
    char help[EDITOR_SCREEN_WIDTH + 1];
    memset(help, ' ', EDITOR_SCREEN_WIDTH);
    memcpy(help, "^S simpan  ^X keluar", 21);
    help[EDITOR_SCREEN_WIDTH] = '\0';

    for (int i = 0; i < EDITOR_SCREEN_WIDTH; i++) {
        vga_write_char(i, EDITOR_HELP_ROW, help[i], COLOR_HELP);
    }
}

/* update scroll biar cursor tetep keliatan */
static void update_scroll(void) {
    int line = get_cursor_line();

    /* scroll ke atas kalo cursor di atas area yang keliatan */
    if (line < scroll_row) {
        scroll_row = line;
    }

    /* scroll ke bawah kalo cursor di bawah area yang keliatan */
    if (line >= scroll_row + EDITOR_TEXT_ROWS) {
        scroll_row = line - EDITOR_TEXT_ROWS + 1;
    }
}

/* gambar seluruh layar */
static void draw_screen(void) {
    update_scroll();
    draw_text_area();
    draw_status_bar();
    draw_help_bar();

    /* posisiin cursor vga */
    int line = get_cursor_line();
    int col = get_cursor_col();
    int screen_row = line - scroll_row;

    if (screen_row >= 0 && screen_row < EDITOR_TEXT_ROWS && col < EDITOR_SCREEN_WIDTH) {
        vga_set_cursor(col, screen_row);
    }
}

/* load file ke buffer */
static int load_file(const char *path) {
    int fd = vfs_open(path, VFS_O_READ);
    if (fd < 0) {
        return -1; /* file gak ada atau gak bisa dibuka */
    }

    buf_used = 0;
    int n;
    while ((n = vfs_read(fd, &text_buf[buf_used], EDITOR_BUFFER_SIZE - buf_used - 1)) > 0) {
        buf_used += n;
        if (buf_used >= EDITOR_BUFFER_SIZE - 1) {
            break;
        }
    }

    vfs_close(fd);
    return 0;
}

/* simpan buffer ke file */
static int save_file(const char *path) {
    int fd = vfs_open(path, VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC);
    if (fd < 0) {
        return -1;
    }

    int written = vfs_write(fd, text_buf, buf_used);
    vfs_close(fd);

    if (written < 0) {
        return -1;
    }

    modified = 0;
    return 0;
}

/* loop utama editor */
void editor_run(const char *path) {
    /* inisialisasi state */
    buf_used = 0;
    cursor_pos = 0;
    scroll_row = 0;
    modified = 0;

    /* copy path */
    uint32_t path_len = strlen(path);
    if (path_len >= MAX_PATH_LENGTH) {
        path_len = MAX_PATH_LENGTH - 1;
    }
    memcpy(file_path, path, path_len);
    file_path[path_len] = '\0';

    /* coba load file (mungkin belum ada) */
    load_file(path);

    /* loop utama */
    int running = 1;
    while (running) {
        draw_screen();

        uint8_t key = keyboard_getkey();

        switch (key) {
            case KEY_CTRL_X:
                /* keluar (auto-save kalo ada perubahan) */
                if (modified) {
                    save_file(path);
                }
                running = 0;
                break;

            case KEY_CTRL_S:
                /* simpan */
                save_file(path);
                break;

            case KEY_ARROW_UP:
                move_cursor_up();
                break;

            case KEY_ARROW_DOWN:
                move_cursor_down();
                break;

            case KEY_ARROW_LEFT:
                move_cursor_left();
                break;

            case KEY_ARROW_RIGHT:
                move_cursor_right();
                break;

            case KEY_HOME:
                move_cursor_home();
                break;

            case KEY_END:
                move_cursor_end();
                break;

            case '\b':
                backspace();
                break;

            case KEY_DELETE:
                delete_char();
                break;

            case '\n':
                insert_newline();
                break;

            case '\t':
                /* tab -> 4 spasi */
                insert_char(' ');
                insert_char(' ');
                insert_char(' ');
                insert_char(' ');
                break;

            default:
                /* karakter yang bisa di-print */
                if (key >= 32 && key < 127) {
                    insert_char((char)key);
                }
                break;
        }
    }

    /* bersihin layar sebelum balik ke shell */
    vga_clear();
}
