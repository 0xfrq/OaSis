# interrupt handling

dokumentasi ini ngebahas gimana OaSis handle interrupt.

## daftar isi

- [apa itu interrupt](#apa-itu-interrupt)
- [idt (interrupt descriptor table)](#idt-interrupt-descriptor-table)
- [pic (programmable interrupt controller)](#pic-programmable-interrupt-controller)
- [isr (interrupt service routine)](#isr-interrupt-service-routine)
- [jenis interrupt](#jenis-interrupt)
- [contoh: keyboard interrupt](#contoh-keyboard-interrupt)

---

## apa itu interrupt

**interrupt** adalah sinyal ke cpu buat stop kerjaan sekarang dan handle sesuatu yang lebih penting.

### kenapa butuh interrupt?

tanpa interrupt:
- cpu harus polling device terus (buang-buang cycle)
- response time lambat
- inefficient

dengan interrupt:
- device signal cpu kalo ada event
- cpu handle event immediately
- efficient dan responsive

### jenis interrupt

1. **hardware interrupt** - dari device external
   - keyboard press
   - timer tick
   - disk ready
   
2. **software interrupt** - dari aplikasi
   - system call (int 0x80)
   - debugging (int 3)
   
3. **exception** - error condition
   - divide by zero
   - page fault
   - general protection fault

## idt (interrupt descriptor table)

**idt** adalah table yang mapping interrupt number ke handler function.

### struktur idt

```c
typedef struct {
    uint16_t offset_low;   // bits 0-15 of handler address
    uint16_t selector;     // code segment selector
    uint8_t zero;          // unused
    uint8_t type_attr;     // type and attributes
    uint16_t offset_high;  // bits 16-31 of handler address
} __attribute__((packed)) idt_entry_t;
```

### idt table

```c
idt_entry_t idt[256];  // 256 interrupt vectors
```

### setup idt entry

```c
void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags) {
    idt[num].offset_low = handler & 0xFFFF;
    idt[num].offset_high = (handler >> 16) & 0xFFFF;
    idt[num].selector = selector;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
}
```

### load idt

```c
void idt_load(void) {
    idt_ptr_t idt_ptr;
    idt_ptr.limit = sizeof(idt) - 1;
    idt_ptr.base = (uint32_t)&idt;
    asm volatile("lidt (%0)" : : "r"(&idt_ptr));
}
```

## pic (programmable interrupt controller)

**pic** adalah chip yang manage hardware interrupt.

### kenapa butuh pic?

- cpu cuma punya 1 pin buat interrupt
- banyak device yang mau interrupt
- pic multiplex multiple device ke 1 pin

### pic di x86

x86 punya 2 pic (master + slave):

```
master pic (irq 0-7)
  ├─ irq 0: timer
  ├─ irq 1: keyboard
  ├─ irq 2: cascade ke slave
  ├─ irq 3: com2
  ├─ irq 4: com1
  ├─ irq 5: lpt2
  ├─ irq 6: floppy
  └─ irq 7: lpt1

slave pic (irq 8-15)
  ├─ irq 8: rtc
  ├─ irq 9: free
  ├─ irq 10: free
  ├─ irq 11: free
  ├─ irq 12: mouse
  ├─ irq 13: fpu
  ├─ irq 14: primary ata
  └─ irq 15: secondary ata
```

### remapping pic

secara default, pic map irq 0-15 ke interrupt 8-15 (bentrok dengan cpu exception). kita remap ke 32-47:

```c
void pic_remap(void) {
    // save masks
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);
    
    // start initialization
    outb(PIC1_CMD, 0x11);
    outb(PIC2_CMD, 0x11);
    
    // set vector offsets
    outb(PIC1_DATA, 0x20);  // irq 0-7 -> int 32-39
    outb(PIC2_DATA, 0x28);  // irq 8-15 -> int 40-47
    
    // setup cascading
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);
    
    // 8086 mode
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);
    
    // restore masks
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}
```

## isr (interrupt service routine)

**isr** adalah function yang dipanggil pas interrupt terjadi.

### struktur isr

```asm
; isr template
isr_common_stub:
    ; 1. save all registers
    pusha
    
    ; 2. call c handler
    push esp          ; push pointer to registers
    call isr_handler
    add esp, 4        ; clean up
    
    ; 3. restore all registers
    popa
    
    ; 4. return from interrupt
    iret
```

### c handler

```c
void isr_handler(registers_t *regs) {
    if (regs->int_no < 32) {
        // cpu exception
        handle_exception(regs);
    } else if (regs->int_no >= 32 && regs->int_no < 48) {
        // hardware interrupt
        handle_irq(regs);
    } else {
        // software interrupt
        handle_software_interrupt(regs);
    }
}
```

## jenis interrupt

### cpu exceptions (int 0-31)

| no | nama | deskripsi |
|----|------|-----------|
| 0 | divide by zero | division by zero |
| 1 | debug | debug exception |
| 2 | nmi | non-maskable interrupt |
| 3 | breakpoint | breakpoint (int 3) |
| 4 | overflow | overflow (into) |
| 5 | bound range | bound range exceeded |
| 6 | invalid opcode | invalid opcode |
| 7 | device not available | fpu not available |
| 8 | double fault | exception during exception handler |
| 10 | invalid tss | invalid tss |
| 11 | segment not present | segment not present |
| 12 | stack fault | stack segment fault |
| 13 | general protection | general protection fault |
| 14 | page fault | page fault |
| 16 | x87 fpu error | x87 fpu floating-point error |
| 17 | alignment check | alignment check |
| 18 | machine check | machine check |
| 19 | simd fpu exception | simd floating-point exception |

### hardware interrupt (int 32-47)

| irq | int | device |
|-----|-----|--------|
| 0 | 32 | timer (pit) |
| 1 | 33 | keyboard |
| 2 | 34 | cascade (slave pic) |
| 3 | 35 | com2 |
| 4 | 36 | com1 |
| 5 | 37 | lpt2 |
| 6 | 38 | floppy |
| 7 | 39 | lpt1 |
| 8 | 40 | rtc |
| 12 | 44 | mouse |
| 14 | 46 | primary ata |
| 15 | 47 | secondary ata |

### software interrupt (int 128 / 0x80)

system call interrupt.

## contoh: keyboard interrupt

### flow

```
user press key
  ↓
keyboard controller generate irq 1
  ↓
pic forward ke cpu sebagai int 33
  ↓
cpu lompat ke isr 33
  ↓
isr save registers
  ↓
isr call keyboard_handler()
  ↓
keyboard_handler read scancode
  ↓
keyboard_handler convert ke ascii
  ↓
keyboard_handler store di buffer
  ↓
isr send eoi ke pic
  ↓
isr restore registers
  ↓
iret
```

### code

```c
void keyboard_handler(void) {
    uint8_t scancode = inb(0x60);
    
    // convert scancode to ascii
    char c = scancode_to_ascii(scancode);
    
    // store in buffer
    if (c != 0) {
        keyboard_buffer[keyboard_buffer_head] = c;
        keyboard_buffer_head = (keyboard_buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
    }
    
    // send eoi to pic
    pic_send_eoi(1);  // irq 1
}
```

---

**kembali ke:** [kernel →](readme.md)
