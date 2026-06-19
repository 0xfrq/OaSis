#ifndef CODEGEN_H
#define CODEGEN_H

#include "parser.h"
#include <stdint.h>

/* Code generator state */
typedef struct {
    char *output;          /* buffer untuk assembly output */
    int output_size;       /* ukuran total buffer */
    int output_pos;        /* posisi tulis saat ini */
    int label_counter;     /* untuk generate unique labels */
    int has_error;

    /* Variable scope - stack frame offsets */
    int local_offset;      /* current top of stack */
    int max_offset;        /* max stack needed for this function */

    /* Current function name (for label prefixes) */
    char current_func[64];
} codegen_t;

/* Initialize code generator */
codegen_t *codegen_create(void);
void codegen_destroy(codegen_t *cg);

/* Generate assembly from AST */
int codegen_program(codegen_t *cg, ast_node_t *program);

/* Get the generated assembly output */
const char *codegen_get_output(codegen_t *cg);

#endif