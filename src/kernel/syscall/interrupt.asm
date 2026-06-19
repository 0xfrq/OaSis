; Handler interupsi OaSis
; ISR = CPU exceptions 0-31
; IRQ = Hardware interrupts 32-47

[EXTERN interrupt_handler]
[EXTERN timer_interrupt_handler]
[EXTERN keyboard_interrupt_handler]
[EXTERN current_task]
[EXTERN task_switch]
[EXTERN int_80_handler]
[EXTERN user_exit_flag]
[EXTERN user_exit_eip]
[EXTERN user_exit_esp]

; CPU exceptions without error code
%macro ISR_NOERRCODE 1
[GLOBAL isr_%1]
isr_%1:
    push byte 0
    push byte %1
    jmp isr_common_stub
%endmacro

; CPU exceptions WITH error code
%macro ISR_ERRCODE 1
[GLOBAL isr_%1]
isr_%1:
    push byte %1
    jmp isr_common_stub
%endmacro

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

isr_common_stub:
    pusha
    mov eax, ds
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov eax, [esp + 40]   ; err_code
    push eax
    mov eax, [esp + 40]   ; int_num
    push eax
    call interrupt_handler
    add esp, 8
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    add esp, 8
    iret

; IRQ handlers
[EXTERN timer_interrupt_handler]
[EXTERN current_task]
[EXTERN kernel_page_dir]

[GLOBAL irq_0]
irq_0:
    cli
    pusha
    push ds
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    call timer_interrupt_handler
    mov al, 0x20
    out 0x20, al
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
    push ds
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    call keyboard_interrupt_handler
    mov al, 0x20
    out 0x20, al
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    popa
    sti
    iret

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

; ============================================================
; int 0x80 syscall handler
;
; CPU-push dari ring 3: SS_user, user_ESP, EFLAGS, CS_user, EIP_user
; CPU-push dari ring 0: EIP, CS, EFLAGS
;
; PUSHA: [esp+0]=EDI, [esp+4]=ESI, [esp+8]=EBP, [esp+12]=old_ESP
;        [esp+16]=EBX, [esp+20]=EDX, [esp+24]=ECX, [esp+28]=EAX
;
; Untuk ring 3:
;   [esp+32]=EIP_user, [esp+36]=CS_user, [esp+40]=EFLAGS
;   [esp+44]=user_ESP, [esp+48]=SS_user
;
; Untuk ring 0:
;   [esp+32]=EIP, [esp+36]=CS, [esp+40]=EFLAGS
; ============================================================

[GLOBAL int_80_wrapper]
int_80_wrapper:
    cli
    pusha

    ; Deteksi ring: CS di [esp+36]. Ring 3 = 0x1B, Ring 0 = 0x08
    cmp dword [esp + 36], 0x08
    je .ring0

; ---- RING 3 ----
    mov eax, [esp + 28]    ; syscall_num
    mov ebx, [esp + 16]    ; arg1
    mov ecx, [esp + 24]    ; arg2
    mov edx, [esp + 20]    ; arg3

    push edx
    push ecx
    push ebx
    push eax
    call int_80_handler
    add esp, 16

    mov [esp + 28], eax    ; return value ke EAX pusha

    ; Cek exit request
    cmp dword [user_exit_flag], 0
    je .r3_noexit

.r3_exit:
    ; Redirect iret ke kernel mode (ring 0)
    mov eax, [user_exit_eip]
    mov ebx, [user_exit_esp]
    mov [esp + 32], eax    ; EIP
    mov dword [esp + 36], 0x08  ; CS = kernel code
    mov dword [esp + 40], 0x202 ; EFLAGS
    mov [esp + 44], ebx    ; ESP = kernel stack
    mov dword [esp + 48], 0x10  ; SS = kernel data
    mov dword [user_exit_flag], 0

.r3_noexit:
    popa
    ; Stack: EIP_user, CS_user, EFLAGS, user_ESP, SS_user
    sti
    iret

; ---- RING 0 ----
.ring0:
    mov eax, [esp + 28]    ; syscall_num
    mov ebx, [esp + 16]    ; arg1
    mov ecx, [esp + 24]    ; arg2
    mov edx, [esp + 20]    ; arg3

    push edx
    push ecx
    push ebx
    push eax
    call int_80_handler
    add esp, 16

    mov [esp + 28], eax

    popa
    ; Stack: EIP, CS, EFLAGS
    sti
    iret

; User exit stub — setelah iret redirect, CPU balik ke sini
; dari user mode. Fungsi ini return ke kernel_main shell loop.
[GLOBAL user_return_to_shell]
[EXTERN kernel_page_dir]
user_return_to_shell:
    ; Setelah iret redirect dari user mode, kernel page dir harus dipulihkan
    mov eax, kernel_page_dir
    mov cr3, eax
    ; ESP = user_exit_esp = EBP of run_user_test
    ; [EBP] = saved EBP of kernel_main
    ; [EBP+4] = return address to kernel_main
    mov esp, [user_exit_esp]
    pop ebp
    ret

; Load IDT function
[GLOBAL load_idt]
load_idt:
    mov eax, [esp + 4]
    lidt [eax]
    ret
