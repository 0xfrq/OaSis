#ifndef TASK_USER_H
#define TASK_USER_H

#include <stdint.h>
#include "task.h"

/*
 * task_user.h - User mode task management
 *
 * Fungsi buat bikin task yang jalan di ring 3 dan
 * fitur switch dari ring 0 ke ring 3.
 */

/* Bikin user task dari buffer kode mesin */
task_t *task_create_user(const char *name, void *code_start, uint32_t code_size);

/* Switch dari kernel mode ke user mode */
void switch_to_user(uint32_t eip, uint32_t esp);

/* Fungsi test: compile kode assembly, jalanin di user mode */
void run_user_test(const char *asm_code);

#endif
