#include "parser.h"
#include "vga.h"
#include "string.h"
#include <stddef.h>

/* Global static pool to avoid malloc */
ast_node_t ast_pool[AST_POOL_SIZE];
int ast_pool_index = 0;

ast_node_t *ast_new_node(ast_type_t type) {
    if (ast_pool_index >= AST_POOL_SIZE) {
        vga_print("parser: AST pool exhausted!\n");
        return 0; /* NULL */
    }
    ast_node_t *node = &ast_pool[ast_pool_index++];
    memset(node, 0, sizeof(ast_node_t));
    node->type = type;
    return node;
}

void ast_pool_reset(void) {
    ast_pool_index = 0;
}

/* Forward declarations for parsing functions */
static void parser_advance(parser_t *p);
static ast_node_t *parse_function(parser_t *p);
static ast_node_t *parse_compound(parser_t *p);
static ast_node_t *parse_statement(parser_t *p);
static ast_node_t *parse_declaration(parser_t *p);
static ast_node_t *parse_if(parser_t *p);
static ast_node_t *parse_while(parser_t *p);
static ast_node_t *parse_for(parser_t *p);
static ast_node_t *parse_return(parser_t *p);
static ast_node_t *parse_expression(parser_t *p);
static ast_node_t *parse_assignment(parser_t *p);
static ast_node_t *parse_comparison(parser_t *p);
static ast_node_t *parse_additive(parser_t *p);
static ast_node_t *parse_term(parser_t *p);
static ast_node_t *parse_factor(parser_t *p);

static void parser_advance(parser_t *p) {
    p->current_token = p->peek_token;
    p->peek_token = lexer_next_token(p->lexer);
}

static void parser_eat(parser_t *p, token_type_t type) {
    if (p->current_token.type == type) {
        parser_advance(p);
    } else {
        char buf[16];
        vga_print("parser error: expected token type ");
        itoa(type, buf, 10);
        vga_print(buf);
        vga_print(" but got ");
        itoa(p->current_token.type, buf, 10);
        vga_print(" '");
        vga_print(p->current_token.value);
        vga_print("' at line ");
        itoa(p->current_token.line, buf, 10);
        vga_print(buf);
        vga_print("\n");
        p->has_error = 1;
    }
}

/* parse_program = parse_function* */
ast_node_t *parser_parse_program(parser_t *p) {
    ast_node_t *program = ast_new_node(AST_PROGRAM);

    while (p->current_token.type != TOKEN_EOF) {
        /* if we hit an error, try to recover by skipping to next int */
        if (p->has_error) {
            vga_print("parse: skipping token at line ");
            char nbuf[16];
            itoa(p->current_token.line, nbuf, 10);
            vga_print(nbuf);
            vga_print("\n");
            p->has_error = 0;
        }
        if (p->current_token.type != TOKEN_INT && p->current_token.type != TOKEN_CHAR_TYPE && p->current_token.type != TOKEN_VOID) {
            parser_advance(p);
            continue;
        }
        ast_node_t *func = parse_function(p);
        if (!func) {
            /* skip this token and try again */
            parser_advance(p);
            continue;
        }
        /* simple linked list via left pointer for program node */
        if (!program->left) {
            program->left = func;
        } else {
            ast_node_t *cur = program->left;
            while (cur->right) cur = cur->right;
            cur->right = func;
        }
    }
    return program;
}

/* parse_function = 'int' identifier '(' ')' compound */
static ast_node_t *parse_function(parser_t *p) {
    ast_node_t *node = ast_new_node(AST_FUNCTION);

    /* Return type (only int supported for now) */
    if (p->current_token.type == TOKEN_INT || p->current_token.type == TOKEN_CHAR_TYPE || p->current_token.type == TOKEN_VOID) {
        parser_advance(p);
        if (p->current_token.type == TOKEN_STAR) parser_advance(p); /* skip * in char* */
    } else {
        return 0;
    }

    /* Function name */
    if (p->current_token.type == TOKEN_IDENTIFIER) {
        node->string_value = p->current_token.value;
        parser_advance(p);
    } else {
        vga_print("parser: skipping bad function def at line ");
        char nbuf[16];
        itoa(p->current_token.line, nbuf, 10);
        vga_print(nbuf);
        vga_print("\n");
        return 0;  /* skip, don't set error */
    }

    parser_eat(p, TOKEN_LPAREN);

    /* Parse parameters: type name (, type name)* */
    ast_node_t *params = NULL;
    ast_node_t *last_param = NULL;
    if (p->current_token.type != TOKEN_RPAREN) {
        while (1) {
            /* Type (int, char, or char*) */
            if (p->current_token.type != TOKEN_INT && p->current_token.type != TOKEN_CHAR_TYPE) {
                p->has_error = 1;
                break;
            }
            parser_advance(p);
            if (p->current_token.type == TOKEN_STAR) parser_advance(p); /* optional * */

            /* Parameter name */
            if (p->current_token.type != TOKEN_IDENTIFIER) {
                p->has_error = 1;
                break;
            }

            ast_node_t *param = ast_new_node(AST_DECLARATION);
            param->string_value = p->current_token.value;
            parser_advance(p);

            /* Link parameter list via right pointer */
            if (!params) params = param;
            else last_param->right = param;
            last_param = param;

            if (p->current_token.type == TOKEN_COMMA) parser_advance(p);
            else break;
        }
    }
    parser_eat(p, TOKEN_RPAREN);
    node->params = params; /* Need to add params field to ast_node_t */

    node->body = parse_compound(p);
    return node;
}

/* parse_compound = '{' statement* '}' */
static ast_node_t *parse_compound(parser_t *p) {
    ast_node_t *node = ast_new_node(AST_COMPOUND);
    parser_eat(p, TOKEN_LBRACE);

    while (p->current_token.type != TOKEN_RBRACE && p->current_token.type != TOKEN_EOF) {
        ast_node_t *stmt = parse_statement(p);
        if (!stmt) break;

        if (!node->left) {
            node->left = stmt;
        } else {
            ast_node_t *cur = node->left;
            while (cur->right) cur = cur->right;
            cur->right = stmt;
        }

        if (p->has_error) break;
    }
    parser_eat(p, TOKEN_RBRACE);
    return node;
}

/* parse_statement = declaration | assignment | if | while | return | compound | expression ';' */
static ast_node_t *parse_statement(parser_t *p) {
    switch (p->current_token.type) {
        case TOKEN_INT:
        case TOKEN_CHAR_TYPE:
            return parse_declaration(p);
        case TOKEN_IF:
            return parse_if(p);
        case TOKEN_WHILE:
            return parse_while(p);
        case TOKEN_FOR:
            return parse_for(p);
        case TOKEN_RETURN:
            return parse_return(p);
        case TOKEN_LBRACE:
            return parse_compound(p);
        default:
            /* Try expression statement (assignment or function call) */
            {
                ast_node_t *expr = parse_expression(p);
                parser_eat(p, TOKEN_SEMICOLON);
                return expr;
            }
    }
}

/* parse_declaration = 'int' identifier ('=' expression)? ';' */
static ast_node_t *parse_declaration(parser_t *p) {
    ast_node_t *node = ast_new_node(AST_DECLARATION);
    int is_char = (p->current_token.type == TOKEN_CHAR_TYPE);
    parser_eat(p, is_char ? TOKEN_CHAR_TYPE : TOKEN_INT);
    (void)is_char;

    if (p->current_token.type == TOKEN_IDENTIFIER) {
        node->string_value = p->current_token.value;
        parser_advance(p);
    } else {
        p->has_error = 1;
        return node;
    }

    if (p->current_token.type == TOKEN_ASSIGN) {
        parser_advance(p);
        node->right = parse_expression(p);
    }

    parser_eat(p, TOKEN_SEMICOLON);
    return node;
}

/* parse_if = 'if' '(' expression ')' statement ('else' statement)? */
static ast_node_t *parse_if(parser_t *p) {
    ast_node_t *node = ast_new_node(AST_IF);
    parser_eat(p, TOKEN_IF);
    parser_eat(p, TOKEN_LPAREN);
    node->condition = parse_expression(p);
    parser_eat(p, TOKEN_RPAREN);
    node->body = parse_statement(p);

    if (p->current_token.type == TOKEN_ELSE) {
        parser_advance(p);
        node->else_body = parse_statement(p);
    }
    return node;
}

/* parse_while = 'while' '(' expression ')' statement */
static ast_node_t *parse_while(parser_t *p) {
    ast_node_t *node = ast_new_node(AST_WHILE);
    parser_eat(p, TOKEN_WHILE);
    parser_eat(p, TOKEN_LPAREN);
    node->condition = parse_expression(p);
    parser_eat(p, TOKEN_RPAREN);
    node->body = parse_statement(p);
    return node;
}

/* parse_for = 'for' '(' expr? ';' expr? ';' expr? ')' statement */
static ast_node_t *parse_for(parser_t *p) {
    ast_node_t *node = ast_new_node(AST_FOR);
    parser_eat(p, TOKEN_FOR);
    parser_eat(p, TOKEN_LPAREN);

    /* Init statement (optional) */
    if (p->current_token.type != TOKEN_SEMICOLON) {
        node->left = parse_expression(p);
    }
    parser_eat(p, TOKEN_SEMICOLON);

    /* Condition (optional, default true) */
    if (p->current_token.type != TOKEN_SEMICOLON) {
        node->condition = parse_expression(p);
    }
    parser_eat(p, TOKEN_SEMICOLON);

    /* Increment (optional) */
    if (p->current_token.type != TOKEN_RPAREN) {
        node->right = parse_expression(p);
    }
    parser_eat(p, TOKEN_RPAREN);

    node->body = parse_statement(p);
    return node;
}

/* parse_return = 'return' expression? ';' */
static ast_node_t *parse_return(parser_t *p) {
    ast_node_t *node = ast_new_node(AST_RETURN);
    parser_eat(p, TOKEN_RETURN);
    if (p->current_token.type != TOKEN_SEMICOLON) {
        node->left = parse_expression(p);
    }
    parser_eat(p, TOKEN_SEMICOLON);
    return node;
}

/* Pratt-style precedence climbing for expressions */
static ast_node_t *parse_expression(parser_t *p) {
    return parse_assignment(p);
}

static ast_node_t *parse_assignment(parser_t *p) {
    ast_node_t *node = parse_comparison(p);

    if (p->current_token.type == TOKEN_ASSIGN) {
        ast_node_t *assign_node = ast_new_node(AST_ASSIGNMENT);
        assign_node->string_value = node->string_value; /* target */
        assign_node->left = node;
        parser_advance(p);
        assign_node->right = parse_assignment(p); /* Right-associative */
        return assign_node;
    }
    return node;
}

static ast_node_t *parse_comparison(parser_t *p) {
    ast_node_t *node = parse_additive(p);

    while (p->current_token.type == TOKEN_EQUAL ||
           p->current_token.type == TOKEN_NOT_EQUAL ||
           p->current_token.type == TOKEN_LESS ||
           p->current_token.type == TOKEN_LESS_EQUAL ||
           p->current_token.type == TOKEN_GREATER ||
           p->current_token.type == TOKEN_GREATER_EQUAL) {

        ast_node_t *op_node = ast_new_node(AST_BINARY_OP);
        op_node->op = p->current_token.type;
        op_node->left = node;
        parser_advance(p);
        op_node->right = parse_additive(p);
        node = op_node;
    }
    return node;
}

static ast_node_t *parse_additive(parser_t *p) {
    ast_node_t *node = parse_term(p);

    while (p->current_token.type == TOKEN_PLUS || p->current_token.type == TOKEN_MINUS) {
        ast_node_t *op_node = ast_new_node(AST_BINARY_OP);
        op_node->op = p->current_token.type;
        op_node->left = node;
        parser_advance(p);
        op_node->right = parse_term(p);
        node = op_node;
    }
    return node;
}

static ast_node_t *parse_term(parser_t *p) {
    ast_node_t *node = parse_factor(p);

    while (p->current_token.type == TOKEN_STAR || p->current_token.type == TOKEN_SLASH) {
        ast_node_t *op_node = ast_new_node(AST_BINARY_OP);
        op_node->op = p->current_token.type;
        op_node->left = node;
        parser_advance(p);
        op_node->right = parse_factor(p);
        node = op_node;
    }
    return node;
}

static ast_node_t *parse_factor(parser_t *p) {
    token_t tok = p->current_token;

    if (tok.type == TOKEN_STRING_LITERAL) {
        ast_node_t *node = ast_new_node(AST_STRING_LITERAL);
        node->string_value = tok.value;
        parser_advance(p);
        return node;
    }

    if (tok.type == TOKEN_INTEGER_LITERAL) {
        ast_node_t *node = ast_new_node(AST_INTEGER_LITERAL);
        /* convert string to int */
        int val = 0;
        for (int i = 0; tok.value[i]; i++) {
            val = val * 10 + (tok.value[i] - '0');
        }
        node->int_value = val;
        parser_advance(p);
        return node;
    }

    if (tok.type == TOKEN_IDENTIFIER) {
        ast_node_t *node = ast_new_node(AST_IDENTIFIER);
        node->string_value = tok.value;
        parser_advance(p);

        /* Check for array subscript */
        if (p->current_token.type == TOKEN_LBRACKET) {
            ast_node_t *sub_node = ast_new_node(AST_ARRAY_SUBSCRIPT);
            sub_node->string_value = node->string_value; /* array name */
            parser_advance(p);
            sub_node->left = parse_expression(p); /* index */
            parser_eat(p, TOKEN_RBRACKET);
            return sub_node;
        }

        /* Check for function call */
        if (p->current_token.type == TOKEN_LPAREN) {
            ast_node_t *call_node = ast_new_node(AST_CALL);
            call_node->string_value = node->string_value;
            parser_advance(p);

            if (p->current_token.type != TOKEN_RPAREN) {
                ast_node_t *arg = parse_expression(p);
                call_node->left = arg;
                while (p->current_token.type == TOKEN_COMMA) {
                    parser_advance(p);
                    ast_node_t *next = parse_expression(p);
                    /* link args via right pointer */
                    if (!call_node->left) {
                        call_node->left = next;
                    } else {
                        ast_node_t *cur = call_node->left;
                        while (cur->right) cur = cur->right;
                        cur->right = next;
                    }
                }
            }
            parser_eat(p, TOKEN_RPAREN);
            return call_node;
        }
        return node;
    }

    if (tok.type == TOKEN_LPAREN) {
        parser_advance(p);
        ast_node_t *node = parse_expression(p);
        parser_eat(p, TOKEN_RPAREN);
        return node;
    }

    vga_print("parser error: unexpected token '");
    vga_print(tok.value);
    vga_print("' in expression\n");
    p->has_error = 1;
    return 0;
}

/* Print AST for debugging */
void ast_print(ast_node_t *node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) vga_print("  ");

    switch (node->type) {
        case AST_PROGRAM:      vga_print("Program\n"); break;
        case AST_FUNCTION:     vga_print("Function: "); vga_print(node->string_value); vga_print("\n"); break;
        case AST_DECLARATION:  vga_print("Decl: "); vga_print(node->string_value); vga_print("\n"); break;
        case AST_ASSIGNMENT:   vga_print("Assign: "); vga_print(node->string_value); vga_print("\n"); break;
        case AST_RETURN:       vga_print("Return\n"); break;
        case AST_IF:           vga_print("If\n"); break;
        case AST_WHILE:        vga_print("While\n"); break;
        case AST_FOR:          vga_print("For\n"); break;
        case AST_ARRAY_SUBSCRIPT: vga_print("ArraySub[]\n"); break;
        case AST_CALL:         vga_print("Call: "); vga_print(node->string_value); vga_print("\n"); break;
        case AST_COMPOUND:     vga_print("Compound\n"); break;
        case AST_INTEGER_LITERAL: {
            vga_print("Int: ");
            char buf[16];
            itoa(node->int_value, buf, 10);
            vga_print(buf);
            vga_print("\n");
            break;
        }
        case AST_STRING_LITERAL:
            vga_print("String: \"");
            vga_print(node->string_value);
            vga_print("\"\n");
            break;
        case AST_IDENTIFIER:   vga_print("Id: "); vga_print(node->string_value); vga_print("\n"); break;
        case AST_BINARY_OP: {
            vga_print("Op: ");
            switch (node->op) {
                case TOKEN_PLUS:  vga_print("+\n"); break;
                case TOKEN_MINUS: vga_print("-\n"); break;
                case TOKEN_STAR:  vga_print("*\n"); break;
                case TOKEN_SLASH: vga_print("/\n"); break;
                case TOKEN_EQUAL: vga_print("==\n"); break;
                case TOKEN_NOT_EQUAL: vga_print("!=\n"); break;
                case TOKEN_LESS:  vga_print("<\n"); break;
                case TOKEN_LESS_EQUAL: vga_print("<=\n"); break;
                case TOKEN_GREATER: vga_print(">\n"); break;
                case TOKEN_GREATER_EQUAL: vga_print(">=\n"); break;
                default: vga_print("?\n"); break;
            }
            break;
        }
    }

    if (node->condition) {
        for (int i = 0; i < indent + 1; i++) vga_print("  ");
        vga_print("Cond:\n");
        ast_print(node->condition, indent + 2);
    }
    if (node->left) {
        ast_print(node->left, indent + 1);
    }
    if (node->right && node->type != AST_FUNCTION) {
        ast_print(node->right, indent + 1);
    }
    if (node->body) {
        for (int i = 0; i < indent + 1; i++) vga_print("  ");
        vga_print("Body:\n");
        ast_print(node->body, indent + 2);
    }
    if (node->else_body) {
        for (int i = 0; i < indent + 1; i++) vga_print("  ");
        vga_print("Else:\n");
        ast_print(node->else_body, indent + 2);
    }
}
