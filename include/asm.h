#ifndef ASM_H
#define ASM_H

#include <stdint.h>

/* assemble dan jalankan kode assembly */
int asm_run(void);

/* assemble kode dari string */
int asm_assemble(const char *code, void **exec_addr);

/* assemble kode dari string untuk user mode (remap libc ke usr_ variants) */
int asm_assemble_user(const char *code, void **exec_addr);

/* assemble dan jalankan file dari disk */
int asm_run_file(const char *path);

#endif