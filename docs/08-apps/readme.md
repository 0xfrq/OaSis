---
layout: default
title: Aplikasi
---

# 08. Aplikasi

## Built-in Text Editor

The editor (`editor.c`) provides nano-like functionality:
- Arrow key navigation
- Ctrl+S to save
- Ctrl+X to exit
- Tab inserts 4 spaces
- Status bar with filename and cursor position
- Characters displayed via VGA directly

## Built-in Assembler

The assembler (`asm.c`) is a full x86 32-bit assembler integrated into the kernel.

### Supported Instructions

| Category | Instructions |
|----------|-------------|
| Data mov | `mov` (reg/reg, reg/mem, mem/reg, imm, seg reg) |
| Arithmetic | `add`, `sub`, `cmp`, `xor`, `and`, `or` |
| Control | `jmp` (near + short), `je/jz`, `jne/jnz`, `jg/jl/jge/jle`, `call`, `ret` |
| Stack | `push` (reg + imm + label), `pop`, `pusha`, `popa` |
| Other | `int`, `nop`, `hlt`, `sti`, `cli`, `inc`, `dec`, `imul`, `idiv`, `cdq`, `test`, `movzx`, `neg`, `setcc`, `cmovcc` |
| Data | `db` (bytes + mixed string/number) |
| Directives | `times N <instruction>` |

### External Symbols
The assembler resolves `_printf`, `_malloc`, `_sys_open`, etc. to kernel functions via a hardcoded symbol table.

### Assembling Flow
1. `asm_assemble(code, &exec_addr)`:
2. Tokenize lines, parse labels and instructions.
3. Emit machine code to `code_buf`.
4. Resolve forward label references via `apply_patches()`.
5. Allocate physical page at CODE_VIRT (0x40000000), copy machine code.
6. Return executable address.

## occ C Compiler

The C compiler (`codegen.c` + `lexer.c` + `parser.c`) compiles a subset of C to x86 assembly.

### Supported Features
- **Types**: `int` (32-bit), `char` (8-bit, stored as 32-bit)
- **Control flow**: `if/else`, `while`, `for`
- **Functions**: Declaration, parameters, return values, nested calls
- **Variables**: Declaration with/without init, assignment, array subscript `arr[i]`
- **Strings**: String literals in printf, stored as inline data
- **Operators**: `+ - * /`, `== != < > <= >=`
- **External functions**: printf, malloc, free, calloc, realloc, sys_open, sys_read, sys_write_fd, sys_close

### Compilation Flow
1. `occ file.c` reads the C source.
2. Lexer tokens the input (identifiers, keywords, numbers, strings, operators).
3. Parser builds an AST (Abstract Syntax Tree).
4. Codegen walks the AST, emits x86 assembly.
5. Assembly written to `/tmp.s`.
6. Assembler runs `/tmp.s` via `nasm`.
7. Code executes at CODE_VIRT.

### Code Generation Examples

**printf("hello")** generates:
```asm
  jmp .L1_strend
.L0_str:
  db 104, 101, 108, 108, 111, 0   ; "hello"
.L1_strend:
  mov eax, .L0_str
  push eax
  call _printf
  add esp, 4
```

**int a = 42; printf("%d", a)** generates:
```asm
  mov eax, 42
  mov [ebp - 4], eax     ; store to 'a'
  mov eax, [ebp - 4]     ; load 'a'
  push eax
  ; format string...
  call _printf
  add esp, 8
```
