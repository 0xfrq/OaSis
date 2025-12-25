; Interrupt handlers for Day 4
; ISR = Interrupt Service Routine (CPU exceptions 0-31)
; IRQ = Interrupt ReQuest (Hardware interrupts 32-47)

[EXTERN interrupt_handler]
[EXTERN timer_interrupt_handler]
[EXTERN keyboard_interrupt_handler]
[EXTERN current_task]
[EXTERN task_switch]

; Macro for CPU exceptions (no error code)
%macro ISR_NOERRCODE 1
[GLOBAL isr_%1]
isr_%1:
    push byte 0              ; Dummy error code
    push byte %1             ; Interrupt number
    jmp isr_common_stub
%endmacro

; Macro for CPU exceptions (with error code)
%macro ISR_ERRCODE 1
[GLOBAL isr_%1]
isr_%1:
    push byte %1             ; Interrupt number (error code already on stack)
    jmp isr_common_stub
%endmacro

; Create exception handlers for interrupts 0-31
ISR_NOERRCODE 0
ISR_NOERRCODE 1
ISR_NOERRCODE 2
ISR_NOERRCODE 3
ISR_NOERRCODE 4
ISR_NOERRCODE 5
ISR_NOERRCODE 6
ISR_NOERRCODE 7
ISR_ERRCODE 8
ISR_NOERRCODE 9
ISR_ERRCODE 10
ISR_ERRCODE 11
ISR_ERRCODE 12
ISR_ERRCODE 13
ISR_ERRCODE 14
ISR_NOERRCODE 15
ISR_NOERRCODE 16
ISR_NOERRCODE 17
ISR_NOERRCODE 18
ISR_NOERRCODE 19
ISR_NOERRCODE 20
ISR_NOERRCODE 21
ISR_NOERRCODE 22
ISR_NOERRCODE 23
ISR_NOERRCODE 24
ISR_NOERRCODE 25
ISR_NOERRCODE 26
ISR_NOERRCODE 27
ISR_NOERRCODE 28
ISR_NOERRCODE 29
ISR_NOERRCODE 30
ISR_NOERRCODE 31

; Common exception handler
isr_common_stub:
    pusha                    ; Push all general registers (eax, ecx, edx, ebx, esp, ebp, esi, edi)
    
    mov eax, ds
    push eax                 ; Push ds
    
    mov ax, 0x10             ; Load kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    call interrupt_handler   ; C function: void interrupt_handler(int int_num, int err_code)
    
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    popa
    add esp, 8               ; Remove error code and interrupt number
    iret

; IRQ handlers (Hardware interrupts 32-47)

[GLOBAL irq_0]
irq_0:
    cli
    pusha
    mov eax, ds
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    call timer_interrupt_handler
    cmp eax, 0
    je .no_switch

.no_switch:    
    mov al, 0x20
    out 0x20, al             ; EOI to master PIC
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    sti
    iret


[GLOBAL irq_1]
irq_1:
    cli
    pusha
    mov eax, ds
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    call keyboard_interrupt_handler
    mov al, 0x20
    out 0x20, al             ; EOI to master PIC
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    sti
    iret

; Stub handlers for other IRQs
%macro STUB_IRQ 1
[GLOBAL irq_%1]
irq_%1:
    cli
    pusha
    mov al, 0x20
    out 0x20, al
    %if %1 >= 8
    out 0xA0, al
    %endif
    popa
    sti
    iret
%endmacro

STUB_IRQ 2
STUB_IRQ 3
STUB_IRQ 4
STUB_IRQ 5
STUB_IRQ 6
STUB_IRQ 7
STUB_IRQ 8
STUB_IRQ 9
STUB_IRQ 10
STUB_IRQ 11
STUB_IRQ 12
STUB_IRQ 13
STUB_IRQ 14
STUB_IRQ 15


; System call hamdler (intx80)

[EXTERN int_80_handler]
[GLOBAL int_80_wrapper]
int_80_wrapper:
    cli
    pusha
    mov edx, ecx
    mov ecx, ebx
    mov ebx, eax
    
    push edx
    push ecx
    push ebx
    push eax

    call int_80_handler

    add esp, 16

    mov [esp+28], eax

    popa
    sti
    iret

; Load IDT function
[GLOBAL load_idt]
load_idt:
    mov eax, [esp + 4]
    lidt [eax]
    ret

