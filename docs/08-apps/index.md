---
layout: default
title: aplikasi
---

# aplikasi

## editor teks (src/kernel/apps/editor.c)

nano-like text editor yang jalan di kernel mode.

### text buffer

```c
static char text_buf[4096]; // buffer teks
static uint32_t buf_used = 0; // jumlah byte terpakai
static uint32_t cursor_pos = 0; // posisi cursor di buffer
```

### key handling

| key | aksi |
|-----|------|
| ctrl+x | exit (simpan dulu kalau ada perubahan) |
| ctrl+s | save ke file via vfs_write |
| arrow up/down | pindah baris |
| arrow left/right | pindah karakter |
| home | ke awal baris |
| end | ke akhir baris |
| backspace | delete karakter sebelumnya |
| delete | delete karakter di cursor |
| enter | insert newline |
| tab | insert 4 spasi |
| printable chars | insert karakter |

### rendering

tampilkan banyak baris dari posisi scroll. setiap baris di-render langsung ke vga buffer. status bar di baris 24 (terakhir) menampilkan filename, line, column.

### save

1. open file dengan `VFS_O_WRITE | VFS_O_CREATE | VFS_O_TRUNC`
2. `vfs_write(fd, text_buf, buf_used)`
3. close fd

## built-in assembler (src/kernel/apps/asm.c)

assembler x86 32-bit lengkap ~banyak baris. menghasilkan machine code yang langsung di-eksekusi.

### alur assembler

```
asm_assemble(code, &exec_addr):
1. copy code ke input_buf[]
2. proses baris per baris:
 - kalau ada ':' -> register label
 - kalau ada instruksi -> parse mnemonic + operand -> emit byte
3. apply patches (forward reference)
4. alloc physical page di CODE_VIRT (0x40000000)
5. copy code_buf ke CODE_VIRT
6. return exec_addr = CODE_VIRT
```

### tabel label

```c
static struct {
 char name[32]; // nama label
 int pos; // posisi byte di code_buf
} labels[MAX_LABELS]; // MAX_LABELS = 32
```

### tabel patch

```c
static struct {
 int pos; /* byte pertama yang perlu di-patch */
 int from; /* posisi setelah instruksi (untuk menghitung relatif) */
 char target[32]; /* nama label tujuan */
 int type; /* 0 = rel8, 1 = rel32, 2 = abs32 */
} patches[MAX_PATCHES]; // MAX_PATCHES = 128
```

### tabel external symbol

```c
typedef struct {
 const char *name; /* nama label (e.g. "_printf") */
 void *addr; /* alamat fungsi di kernel */
} extern_sym_t;
```

assembler mencari external symbol di tabel ini kalau label tidak ketemu di labels[].

### instruksi didukung

mov: reg/reg, reg/mem, mem/reg, imm, seg reg
add/sub/cmp/xor/and/or: reg/reg, mem, imm
jmp/je/jne/jg/jl/jge/jle: short (rel8) atau near (rel32)
call: rel32 (termasuk ke external symbol)
push: reg, imm (8/), label
pop/inc/dec: reg
imul: reg/reg
idiv/cdq: reg
div: unsigned divide
neg: negate
test: logical and, set flags
movzx: mov zero-extend
setcc: set byte on condition (sete, setne, setl, setg, setle, setge, setb, setbe, seta, setae)
cmovcc: conditional move (cmove, cmovne, cmovl, cmovle, cmovg, cmovge)
int: software interrupt
nop/hlt/sti/cli: single byte
pusha/popa: push/pop all registers
db: data bytes (mixed format: string + angka)

### times directive

```c
if (streq(mnem, "times")) {
 uint32_t count = parse_int(ops, ...);
 char *instr = skip_count(ops);
 /* parse instruksi sekali */
 char sub_mnem[16];
 parse_mnemonic(instr, sub_mnem, &sub_ops);

 if (streq(sub_mnem, "db")) {
 /* parse args sekali, emit N kali */
 uint8_t values[128];
 int nv = parse_db_args(sub_ops, values);
 for (uint32_t rep = 0; rep < count; rep++)
 for (int vi = 0; vi < nv; vi++) emit(values[vi]);
 return 0;
 }
 if (streq(sub_mnem, "nop"))
 for (uint32_t rep = 0; rep < count; rep++) emit(0x90);
}
```

### gen_push dengan label

sebelumnya push hanya mendukung register dan immediate. sekarang mendukung label:

```c
uint32_t imm;
if (!parse_int(ops, &imm)) {
 /* mungkin label */
 int target = find_label(ops);
 if (target >= 0) { emit(0x68); emit32(CODE_VIRT + target); return 0; }
 add_patch(code_len + 1, 0, ops, 2); /* forward reference */
 emit(0x68); emit32(0);
 return 0;
}
```

## occ compiler (src/kernel/lib/)

compiler subset c yang terdiri dari beberapa komponen:
1. lexer.c -> tokenizer
2. parser.c -> ast builder
3. codegen.c -> x86 assembly generator

### lexer (lexer.c)

tokenizer yang membaca input karakter per karakter.

```c
typedef enum {
 TOKEN_EOF, TOKEN_IDENTIFIER, TOKEN_NUMBER, TOKEN_STRING,
 TOKEN_INT, TOKEN_CHAR_TYPE, TOKEN_VOID,
 TOKEN_IF, TOKEN_ELSE, TOKEN_WHILE, TOKEN_FOR,
 TOKEN_RETURN, TOKEN_BREAK, TOKEN_CONTINUE,
 TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
 TOKEN_ASSIGN, TOKEN_EQUAL, TOKEN_NOT_EQUAL,
 TOKEN_LESS, TOKEN_LESS_EQUAL, TOKEN_GREATER, TOKEN_GREATER_EQUAL,
 TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE,
 TOKEN_LBRACKET, TOKEN_RBRACKET, TOKEN_SEMICOLON, TOKEN_COMMA,
 TOKEN_INTEGER_LITERAL, TOKEN_STRING_LITERAL, TOKEN_CHAR_LITERAL,
} token_type_t;
```

### parser (parser.c)

recursive descent parser yang menghasilkan ast.

tipe ast node:
```c
typedef enum {
 AST_PROGRAM, AST_FUNCTION, AST_DECLARATION, AST_ASSIGNMENT,
 AST_RETURN, AST_IF, AST_WHILE, AST_FOR, AST_CALL, AST_COMPOUND,
 AST_INTEGER_LITERAL, AST_STRING_LITERAL, AST_IDENTIFIER,
 AST_BINARY_OP, AST_ARRAY_SUBSCRIPT
} ast_type_t;
```

struktur node:
```c
typedef struct ast_node {
 ast_type_t type;
 struct ast_node *left; /* operand kiri / child pertama */
 struct ast_node *right; /* operand kanan / sibling */
 struct ast_node *condition;/* untuk if/while */
 struct ast_node *body; /* untuk function, if, while, for */
 struct ast_node *else_body;/* untuk if-else */
 struct ast_node *params; /* untuk function parameter */
 int int_value;
 char *string_value;
 token_type_t op;
} ast_node_t;
```

### parsing function dengan parameter

```c
static ast_node_t *parse_function(parser_t *p) {
 parser_eat(p, TOKEN_INT); /* atau TOKEN_CHAR_TYPE atau TOKEN_VOID */
 /* optional * untuk pointer type */
 /* ... function name ... */
 parser_eat(p, TOKEN_LPAREN);

 /* parse parameters: type name (, type name)* */
 while (p->current_token.type != TOKEN_RPAREN) {
 parser_eat(p, TOKEN_INT); /* type */
 if (p->current_token.type == TOKEN_STAR) parser_advance(p);
 /* parameter name */
 ast_node_t *param = ast_new_node(AST_DECLARATION);
 param->string_value = p->current_token.value;
 /* link ke parameter list */
 if (p->current_token.type == TOKEN_COMMA) parser_advance(p);
 }
 parser_eat(p, TOKEN_RPAREN);
 node->params = params;
 node->body = parse_compound(p);
}
```

### parsing for loop

```c
static ast_node_t *parse_for(parser_t *p) {
 ast_node_t *node = ast_new_node(AST_FOR);
 parser_eat(p, TOKEN_FOR);
 parser_eat(p, TOKEN_LPAREN);

 /* init statement (optional) */
 if (p->current_token.type != TOKEN_SEMICOLON) node->left = parse_expression(p);
 parser_eat(p, TOKEN_SEMICOLON);

 /* condition (optional, default true) */
 if (p->current_token.type != TOKEN_SEMICOLON) node->condition = parse_expression(p);
 parser_eat(p, TOKEN_SEMICOLON);

 /* increment (optional) */
 if (p->current_token.type != TOKEN_RPAREN) node->right = parse_expression(p);
 parser_eat(p, TOKEN_RPAREN);

 node->body = parse_statement(p);
 return node;
}
```

### parsing array subscript

di `parse_factor`, setelah identifier, kalau ditemukan `[`:
```c
if (p->current_token.type == TOKEN_LBRACKET) {
 ast_node_t *sub = ast_new_node(AST_ARRAY_SUBSCRIPT);
 sub->string_value = node->string_value; /* array name */
 parser_advance(p);
 sub->left = parse_expression(p); /* index */
 parser_eat(p, TOKEN_RBRACKET);
 return sub;
}
```

### codegen (codegen.c)

walk ast node by node, generate x86 assembly.

#### gen_expression untuk AST_STRING_LITERAL

```c
case AST_STRING_LITERAL:
 /* jmp over data */
 emit_str(cg, " jmp "); emit_line(cg, end_label);
 /* data label */
 emit_str(cg, str_label); emit_line(cg, ":");
 /* db 72, 101, 108,... (setiap byte sebagai angka) */
 emit_str(cg, " db ");
 for (int si = 0; string_value[si]; si++) {
 char buf[16]; int_to_str((unsigned char)sc, buf);
 emit_str(cg, buf);
 if (string_value[si+1]) emit_str(cg, ", ");
 }
 emit_line(cg, ", 0");
 /* end label + load address */
 emit_str(cg, end_label); emit_line(cg, ":");
 emit_str(cg, " mov eax, "); emit_line(cg, str_label);
 break;
```

#### gen_function untuk parameter

```c
ast_node_t *param = func->params;
while (param) {
 int pv = alloc_var(param->string_value);
 /* param di [ebp + 8], [ebp + 12], ... */
 char tmp_str[16]; int_to_str(param_offset, tmp_str);
 emit_str(cg, " mov eax, [ebp + "); emit_str(cg, tmp_str); emit_line(cg, "]");
 emit_store_var(cg, pv);
 param = param->right;
 param_offset += 4;
}
```

#### gen_for

```c
/* init */
if (fornode->left) gen_expression(cg, fornode->left);
emit_str(cg, loop_label); emit_line(cg, ":");
/* condition */
if (fornode->condition) gen_expression(cg, fornode->condition);
else emit_line(cg, " mov eax, 1");
emit_line(cg, " cmp eax, 0");
emit_str(cg, " je "); emit_line(cg, end_label);
/* body */
gen_statement(cg, fornode->body);
/* increment */
if (fornode->right) gen_expression(cg, fornode->right);
emit_str(cg, " jmp "); emit_line(cg, loop_label);
emit_str(cg, end_label); emit_line(cg, ":");
```

#### codegen untuk array subscript

```c
case AST_ARRAY_SUBSCRIPT:
 gen_expression(cg, expr->left); /* index */
 emit_line(cg, " shl eax, 2"); /* index * 4 (sizeof int) */
 int v = find_var(expr->string_value);
 if (v >= 0) {
 /* array sebagai local variable */
 char b[16]; int_to_str(v, b);
 emit_str(cg, " lea ebx, [ebp - "); emit_str(cg, b); emit_line(cg, "]");
 emit_line(cg, " mov eax, [ebx + eax]");
 }
 break;
```

### gen_function_call

push args dari kanan ke kiri (cdecl calling convention):
```c
for (int i = n - 1; i >= 0; i--) {
 gen_expression(cg, args[i]);
 emit_line(cg, " push eax");
}
emit_str(cg, " call _"); emit_str(cg, call->string_value); emit_line(cg, "");
if (arg_count > 0) {
 char cleanup[32];
 int_to_str(arg_count * 4, cleanup);
 emit_str(cg, " add esp, "); emit_line(cg, cleanup);
}
```
