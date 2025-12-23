BITS 32
SECTION .multiboot
    align 4
    dd 0x1BADB002
    dd 0x00
    dd -(0x1BADB002)

SECTION .text
GLOBAL _start
EXTERN kernel_main

_start:
    cli
    mov esp, stack_top
    call kernel_main

.hang:
    hlt
    jmp .hang

section .bss
align 16
resb 8192
stack_top: