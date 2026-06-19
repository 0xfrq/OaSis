#include "lexer.h"
#include "string.h"
#include "vga.h"

/* Helper: check if character is whitespace */
static int is_whitespace(char ch) {
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

/* Helper: check if character is letter or underscore */
static int is_letter(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
}

/* Helper: check if character is digit */
static int is_digit(char ch) {
    return ch >= '0' && ch <= '9';
}

/* Move to next character */
static void lexer_read_char(lexer_t *l) {
    if (l->read_position >= l->input_length) {
        l->ch = 0;
    } else {
        l->ch = l->input[l->read_position];
    }
    l->position = l->read_position;
    l->read_position++;

    if (l->ch == '\n') {
        l->line++;
        l->column = 1;
    } else {
        l->column++;
    }
}

/* Peek at next character without advancing */
static char lexer_peek_char(lexer_t *l) {
    if (l->read_position >= l->input_length) {
        return 0;
    }
    return l->input[l->read_position];
}

/* Skip whitespace and comments */
static void lexer_skip_whitespace(lexer_t *l) {
    while (is_whitespace(l->ch)) {
        lexer_read_char(l);
    }

    // Handle comments
    if (l->ch == '/' && lexer_peek_char(l) == '/') {
        // Single line comment
        while (l->ch != '\n' && l->ch != 0) {
            lexer_read_char(l);
        }
        lexer_skip_whitespace(l);
    } else if (l->ch == '/' && lexer_peek_char(l) == '*') {
        // Multi-line comment
        lexer_read_char(l); // skip /
        lexer_read_char(l); // skip *
        while (!(l->ch == '*' && lexer_peek_char(l) == '/') && l->ch != 0) {
            lexer_read_char(l);
        }
        lexer_read_char(l); // skip *
        lexer_read_char(l); // skip /
        lexer_skip_whitespace(l);
    }
}

/* Initialize lexer */
lexer_t *lexer_create(const char *input, int length) {
    static lexer_t l;
    l.input = input;
    l.input_length = length;
    l.position = 0;
    l.read_position = 0;
    l.ch = 0;
    l.line = 1;
    l.column = 0;
    /* Cast away const - we will null-terminate string literals in place.
     * The input buffer must be writable (caller's responsibility). */
    l.input_writable = (char *)input;
    lexer_read_char(&l);
    return &l;
}

void lexer_destroy(lexer_t *lexer) {
    // No-op for static lexer
    (void)lexer;
}

/* Token buffer for values - no longer used since we point to input buffer directly */

/* Core function: get next token */
token_t lexer_next_token(lexer_t *l) {
    token_t tok;
    lexer_skip_whitespace(l);

    tok.line = l->line;
    tok.column = l->column;
    tok.value = ""; // Default

    switch (l->ch) {
        case '=':
            if (lexer_peek_char(l) == '=') {
                char *prev_ch = (char *)&l->input[l->position];
                lexer_read_char(l);
                tok.type = TOKEN_EQUAL;
                tok.value = "==";
            } else {
                tok.type = TOKEN_ASSIGN;
                tok.value = "=";
            }
            break;
        case '+':
            tok.type = TOKEN_PLUS;
            tok.value = "+";
            break;
        case '-':
            tok.type = TOKEN_MINUS;
            tok.value = "-";
            break;
        case '*':
            tok.type = TOKEN_STAR;
            tok.value = "*";
            break;
        case '/':
            tok.type = TOKEN_SLASH;
            tok.value = "/";
            break;
        case '%':
            tok.type = TOKEN_PERCENT;
            tok.value = "%";
            break;
        case '!':
            if (lexer_peek_char(l) == '=') {
                lexer_read_char(l);
                tok.type = TOKEN_NOT_EQUAL;
                tok.value = "!=";
            } else {
                tok.type = TOKEN_LOGICAL_NOT;
                tok.value = "!";
            }
            break;
        case '<':
            if (lexer_peek_char(l) == '=') {
                lexer_read_char(l);
                tok.type = TOKEN_LESS_EQUAL;
                tok.value = "<=";
            } else {
                tok.type = TOKEN_LESS;
                tok.value = "<";
            }
            break;
        case '>':
            if (lexer_peek_char(l) == '=') {
                lexer_read_char(l);
                tok.type = TOKEN_GREATER_EQUAL;
                tok.value = ">=";
            } else {
                tok.type = TOKEN_GREATER;
                tok.value = ">";
            }
            break;
        case '&':
            if (lexer_peek_char(l) == '&') {
                lexer_read_char(l);
                tok.type = TOKEN_LOGICAL_AND;
                tok.value = "&&";
            } else {
                tok.type = TOKEN_ERROR;
                tok.value = "&";
            }
            break;
        case '|':
            if (lexer_peek_char(l) == '|') {
                lexer_read_char(l);
                tok.type = TOKEN_LOGICAL_OR;
                tok.value = "||";
            } else {
                tok.type = TOKEN_ERROR;
                tok.value = "|";
            }
            break;
        case ';':
            tok.type = TOKEN_SEMICOLON;
            tok.value = ";";
            break;
        case ',':
            tok.type = TOKEN_COMMA;
            tok.value = ",";
            break;
        case '(':
            tok.type = TOKEN_LPAREN;
            tok.value = "(";
            break;
        case ')':
            tok.type = TOKEN_RPAREN;
            tok.value = ")";
            break;
        case '{':
            tok.type = TOKEN_LBRACE;
            tok.value = "{";
            break;
        case '}':
            tok.type = TOKEN_RBRACE;
            tok.value = "}";
            break;
        case '[':
            tok.type = TOKEN_LBRACKET;
            tok.value = "[";
            break;
        case ']':
            tok.type = TOKEN_RBRACKET;
            tok.value = "]";
            break;
        case '"':
            tok.type = TOKEN_STRING_LITERAL;
            int start_pos = l->position + 1;
            lexer_read_char(l);
            while (l->ch != '"' && l->ch != 0) {
                lexer_read_char(l);
            }
            int len = l->position - start_pos;
            /* Point directly to the input buffer; null-terminate the string in place */
            l->input_writable[l->position] = 0;
            tok.value = (char *)&l->input[start_pos];
            break;
        case 0:
            tok.type = TOKEN_EOF;
            tok.value = "";
            break;
        default:
            if (is_letter(l->ch)) {
                int start = l->position;
                while (is_letter(l->ch) || is_digit(l->ch)) {
                    lexer_read_char(l);
                }
                int len = l->position - start;
                l->input_writable[l->position] = 0;
                char *ident = (char *)&l->input[start];

                // Keyword detection
                if (strcmp(ident, "int") == 0) tok.type = TOKEN_INT;
                else if (strcmp(ident, "char") == 0) tok.type = TOKEN_CHAR_TYPE;
                else if (strcmp(ident, "void") == 0) tok.type = TOKEN_VOID;
                else if (strcmp(ident, "if") == 0) tok.type = TOKEN_IF;
                else if (strcmp(ident, "else") == 0) tok.type = TOKEN_ELSE;
                else if (strcmp(ident, "while") == 0) tok.type = TOKEN_WHILE;
                else if (strcmp(ident, "for") == 0) tok.type = TOKEN_FOR;
                else if (strcmp(ident, "return") == 0) tok.type = TOKEN_RETURN;
                else if (strcmp(ident, "break") == 0) tok.type = TOKEN_BREAK;
                else if (strcmp(ident, "continue") == 0) tok.type = TOKEN_CONTINUE;
                else tok.type = TOKEN_IDENTIFIER;

                tok.value = ident;
                return tok; // Already advanced
            } else if (is_digit(l->ch)) {
                int start = l->position;
                while (is_digit(l->ch)) {
                    lexer_read_char(l);
                }
                int len = l->position - start;
                tok.type = TOKEN_INTEGER_LITERAL;
                l->input_writable[l->position] = 0;
                tok.value = (char *)&l->input[start];
                return tok; // Already advanced
            } else {
                tok.type = TOKEN_ERROR;
                tok.value = "unknown char";
            }
            break;
    }

    lexer_read_char(l);
    return tok;
}

void token_free(token_t *token) {
    // In this implementation we don't use malloc, so no-op
    (void)token;
}
