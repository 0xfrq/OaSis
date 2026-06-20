---
layout: default
title: aplikasi
---

# aplikasi

## editor teks

di `src/kernel/apps/editor.c`. nano-like editor.

keyboard handling:
- arrow keys -> navigasi
- ctrl+s -> save (vfs_write)
- ctrl+x -> exit
- tab -> 4 spasi
- backspace -> delete
- delete -> delete
- enter -> newline

text buffer: static array 4096 byte.

## built-in assembler

di `src/kernel/apps/asm.c`. assembler x86 32-bit lengkap.

cara kerja:
1. `asm_assemble(code, &exec_addr)` -> tokenize -> parse -> emit machine code
2. label di-register pas ketemu `:` di awal baris
3. forward reference di-patch via tabel patch
4. external symbol resolve via `extern_syms[]`
5. code di-copy ke CODE_VIRT (0x40000000) dan dieksekusi

support:
- mov, add, sub, cmp, xor, and, or, push, pop, call, ret, jmp, je, jne, int, nop, hlt
- times directive
- db dengan mixed format
- segment registers
- label push

## occ compiler

di `src/kernel/lib/`. compiler subset c.

flow: `occ file.c`:
1. lexer: tokenize source -> identifiers, keywords, numbers, strings, operators
2. parser: build ast (abstract syntax tree) -> program, function, statement, expression
3. codegen: walk ast -> generate x86 assembly
4. write ke /tmp.s -> nasm assemble -> execute

support:
- type: int, char
- control: if/else, while, for
- variable declaration + assignment
- function with parameters
- string literals in printf
- arithmetic + comparison
- external symbols: printf, malloc, free, sys_open, sys_read, sys_write_fd

contoh output codegen buat `printf("hello")`:
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
