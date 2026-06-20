#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include <stdint.h>

/* AST Node Types */
typedef enum {
    AST_PROGRAM,
    AST_FUNCTION,
    AST_DECLARATION,
    AST_ASSIGNMENT,
    AST_RETURN,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_ARRAY_SUBSCRIPT,
    AST_CALL,
    AST_COMPOUND, /* { ... } */

    /* Expression types */
    AST_INTEGER_LITERAL,
    AST_STRING_LITERAL,
    AST_IDENTIFIER,
    AST_BINARY_OP
} ast_type_t;

typedef struct ast_node {
    ast_type_t type;
    struct ast_node *left;
    struct ast_node *right;
    struct ast_node *condition; /* For if/while */
    struct ast_node *body;      /* For if/while/function */
    struct ast_node *else_body; /* For if-else */

    int int_value;          /* For literals and op precedence */
    char *string_value;     /* For identifiers and function names */
    token_type_t op;        /* For binary operators */
    struct ast_node *params;  /* For function parameters */
} ast_node_t;

/* Parser Context */
typedef struct {
    lexer_t *lexer;
    token_t current_token;
    token_t peek_token;
    int has_error;
} parser_t;

/* Static memory pool for AST nodes
 * Since OaSis doesn't have malloc, we allocate from a static array.
 * This limits the depth of the AST we can parse, but is good for education. */
#define AST_POOL_SIZE 4096
extern ast_node_t ast_pool[AST_POOL_SIZE];
extern int ast_pool_index;

ast_node_t *ast_new_node(ast_type_t type);
void ast_pool_reset(void);
ast_node_t *parser_parse_program(parser_t *p);

void ast_print(ast_node_t *node, int indent);

#endif