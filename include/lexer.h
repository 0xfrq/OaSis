#ifndef LEXER_H
#define LEXER_H

#include <stdint.h>

/* Token types */
typedef enum {
    TOKEN_EOF = 0,
    TOKEN_IDENTIFIER,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_CHAR,

    /* Keywords */
    TOKEN_INT,
    TOKEN_CHAR_TYPE,
    TOKEN_VOID,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_RETURN,
    TOKEN_BREAK,
    TOKEN_CONTINUE,

    /* Operators */
    TOKEN_PLUS,        // +
    TOKEN_MINUS,       // -
    TOKEN_STAR,        // *
    TOKEN_SLASH,       // /
    TOKEN_PERCENT,     // %

    TOKEN_ASSIGN,      // =
    TOKEN_EQUAL,       // ==
    TOKEN_NOT_EQUAL,   // !=
    TOKEN_LESS,        // <
    TOKEN_LESS_EQUAL,  // <=
    TOKEN_GREATER,     // >
    TOKEN_GREATER_EQUAL, // >=

    TOKEN_LOGICAL_AND, // &&
    TOKEN_LOGICAL_OR,  // ||
    TOKEN_LOGICAL_NOT, // !

    /* Delimiters */
    TOKEN_LPAREN,      // (
    TOKEN_RPAREN,      // )
    TOKEN_LBRACE,      // {
    TOKEN_RBRACE,      // }
    TOKEN_LBRACKET,    // [
    TOKEN_RBRACKET,    // ]
    TOKEN_SEMICOLON,   // ;
    TOKEN_COMMA,       // ,

    /* Literals */
    TOKEN_INTEGER_LITERAL,
    TOKEN_STRING_LITERAL,
    TOKEN_CHAR_LITERAL,

    TOKEN_ERROR
} token_type_t;

typedef struct {
    token_type_t type;
    char *value;
    int line;
    int column;
} token_t;

typedef struct {
    const char *input;
    char *input_writable;  /* same as input but cast for null-termination */
    int input_length;      /* panjang asli input (jangan pakai strlen karena kita null-terminate tokens) */
    int position;
    int read_position;
    char ch;
    int line;
    int column;
} lexer_t;

/* Function prototypes */
lexer_t *lexer_create(const char *input, int length);
void lexer_destroy(lexer_t *lexer);
token_t lexer_next_token(lexer_t *lexer);
void token_print(token_t token);
void token_free(token_t *token);

#endif