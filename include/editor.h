#ifndef EDITOR_H
#define EDITOR_H

/* konfigurasi editor */
#define EDITOR_BUFFER_SIZE 4096
#define EDITOR_MAX_LINES 256
#define EDITOR_TEXT_ROWS 23
#define EDITOR_STATUS_ROW 23
#define EDITOR_HELP_ROW 24
#define EDITOR_SCREEN_WIDTH 80

/* jalankan text editor buat file yang dikasih */
void editor_run(const char *path);

#endif
