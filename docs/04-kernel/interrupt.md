---
layout: default
title: Interrupt Handling
---

# Interrupt Handling

## IDT (Interrupt Descriptor Table)

The IDT has 256 entries, each 8 bytes:
- **Base**: `idt` array in `idt.c` (static allocation).
- **Gate types**: All set as 32-bit interrupt gates (`0x8E`).
- **Selector**: Kernel code segment (`0x08`).

### Setup (`idt_init`)
1. Set `idt_ptr.limit = 256 * 8 - 1`.
2. Set `idt_ptr.base = (uint32_t)&idt`.
3. Install ISRs 0-31 with `idt_set_entry(num, handler, 0x08, 0x8E)`.
4. Install IRQs 32-47 with same attributes.
5. Load IDT via `lidt` instruction.

### Exception Categories
| Range | Type | Examples |
|-------|------|---------|
| 0-7 | General exceptions | Divide-by-zero (0), GPF (13), Page Fault (14) |
| 8-14 | Exceptions with error codes | Double fault (8), Invalid TSS (10), Stack fault (12) |
| 15-31 | Reserved/other | FPU (16), Alignment check (17), SIMD (19) |

## ISR Handler (`isr_common_stub`)

### Stack Layout (ring 0 -> handler)
```text
[ESP+0]  = EDI (from PUSHA)
[ESP+4]  = ESI
[ESP+8]  = EBP
[ESP+12] = old ESP
[ESP+16] = EBX
[ESP+20] = EDX
[ESP+24] = ECX
[ESP+28] = EAX
[ESP+32] = DS (pushed)
[ESP+36] = error_code
[ESP+40] = int_number
[ESP+44] = EIP (CPU push)
[ESP+48] = CS
[ESP+52] = EFLAGS
```

For ring 3 exceptions, the CPU also pushes SS and user_ESP before the error code.

### Handler Flow
1. PUSHA saves all general registers.
2. DS is pushed and reloaded with kernel data segment (`0x10`).
3. Error code and int number are pushed as arguments.
4. C function `interrupt_handler(int_num, err_code)` is called.
5. Arguments are popped, DS is restored, POPA restores registers.
6. IRET returns to the interrupted context.

## Syscall Handler (`int 0x80`)

### Ring Detection
The handler checks `[esp+36]` (CS value from CPU push):
- `0x08` — from ring 0 (kernel). Stack has 3 CPU-pushed words: EIP, CS, EFLAGS.
- `0x1B` — from ring 3 (user). Stack has 5 CPU-pushed words: SS, ESP, EFLAGS, CS, EIP.

### Ring 0 Path
1. Load syscall number and arguments from PUSHA frame.
2. Push arguments to stack, call `int_80_handler()`.
3. Store return value to EAX slot in PUSHA frame.
4. POPA, IRET (pops EIP, CS, EFLAGS — 3 words).

### Ring 3 Path
1. Same argument loading as ring 0.
2. After handler returns, check `user_exit_flag`.
3. If exit requested, overwrite iret frame with kernel return values:
   - EIP = `user_exit_eip` (address of `user_return_to_shell`)
   - CS = `0x08` (kernel code)
   - ESP = `user_exit_esp` (kernel stack)
   - SS = `0x10` (kernel data)
4. POPA, IRET (pops 5 words: EIP, CS, EFLAGS, ESP, SS).

### User Exit Redirection
`user_return_to_shell`:
1. Restore CR3 to `kernel_page_dir`.
2. Restore kernel stack via `user_exit_esp`.
3. POP EBP (restores kernel_main's frame pointer).
4. RET — returns to kernel_main's shell loop.
