# Day 4 – Oasis OS: Hardware Interrupts & Timer (PIT)

## Goal of Day 4

Transform Oasis from a **polling-based system** to a **hardware interrupt-driven system**.

On Day 3, we used polling:
- Keyboard driver continuously reads hardware
- CPU wastes cycles checking for input
- System is responsive but inefficient

On Day 4, we switch to **interrupts**:
- Hardware signals CPU when events occur
- CPU only runs handler code when needed
- System is efficient and responsive

By the end of Day 4, we must be able to say:

- The CPU has an Interrupt Descriptor Table (IDT)
- Interrupts are properly masked/unmasked
- PIT (timer) generates IRQ 0 every ~10ms
- Keyboard uses IRQ 1 instead of polling
- Uptime counter increments with each tick
- System is stable with hardware interrupts

This is the moment Oasis becomes a **real-time system**.

---

## Critical Concepts

### What is an Interrupt?

An **interrupt** is a hardware signal that forces the CPU to stop what it's doing and run a handler.

Execution flow:

```
Normal code execution
        ↓
[Hardware signal arrives]
        ↓
CPU saves current state (registers, instruction pointer)
        ↓
CPU jumps to interrupt handler
        ↓
Handler runs
        ↓
Handler returns via iret (return from interrupt)
        ↓
CPU restores saved state
        ↓
Normal code continues
```

### Why Interrupts Matter

1. **Efficiency** – CPU only wakes up when needed
2. **Fairness** – Timer interrupt ensures all tasks get CPU time
3. **Responsiveness** – Hardware events are immediate, not polled
4. **Real OS Foundation** – Every real OS uses interrupts

### x86 Interrupt Architecture

On 32-bit x86:

- 256 interrupts (0-255)
- Interrupts 0-31 are reserved for CPU exceptions
- Interrupts 32-47 are hardware IRQs (from PIC)
- Interrupts 48-255 are available for OS

**IRQ Mapping (PIC):**

```
PIC Master (IRQs 0-7)           PIC Slave (IRQs 8-15)
├── IRQ 0: Timer (PIT)          ├── IRQ 8:  RTC
├── IRQ 1: Keyboard             ├── IRQ 9:  IRQ2 cascade
├── IRQ 2: IRQ 8-15 cascade     ├── IRQ 10: reserved
├── IRQ 3: COM2/4               ├── IRQ 11: reserved
├── IRQ 4: COM1/3               ├── IRQ 12: PS/2 Mouse
├── IRQ 5: LPT2                 ├── IRQ 13: Coprocessor
├── IRQ 6: Floppy               ├── IRQ 14: ATA Primary
└── IRQ 7: LPT1                 └── IRQ 15: ATA Secondary
```

For today, we care about:
- **IRQ 0** (Timer) → maps to interrupt 32
- **IRQ 1** (Keyboard) → maps to interrupt 33

---

## Part 1: Understanding IDT (Interrupt Descriptor Table)

The **IDT** is a CPU data structure that maps interrupt numbers to handler functions.

Structure of one IDT entry (8 bytes):

```
Offset     15..14 13 12    11..8  7    6  5  4   3..0
┌──────────┬──────┬──┬──────┬────┬─────┬────────────┐
│ Offset   │ DPL  │ S│ Type │ x  │ 0   │ Segment    │
│ 31..16   │      │  │      │    │     │ Selector   │
│ (15..0)  │      │  │      │    │     │ 31..16     │
└──────────┴──────┴──┴──────┴────┴─────┴────────────┘
 Offset   DPL  S  Type x  0  SegSel
 63..48   55   4  43..40 12 10  31..16
```

Simpler view:

```c
struct IDTEntry {
    uint16_t offset_lo;        // Handler address (15..0)
    uint16_t selector;         // Code segment selector (kernel = 0x08)
    uint8_t  ist;              // Reserved (0)
    uint8_t  type_attr;        // Type & attributes (0x8E = trap, present, ring 0)
    uint16_t offset_hi;        // Handler address (31..16)
};
```

For our kernel:

```
Interrupt 0-31:   CPU exceptions (we'll handle later)
Interrupt 32:     IRQ 0 (Timer)      → timer_handler
Interrupt 33:     IRQ 1 (Keyboard)   → keyboard_handler
```

---

## Part 2: PIT (Programmable Interval Timer)

The **PIT** is the system timer. It generates a clock signal at regular intervals.

**PIT Ports (I/O):**

```
Port 0x40: Data (channel 0)
Port 0x41: Data (channel 1)
Port 0x42: Data (channel 2)
Port 0x43: Control
```

**PIT Frequency:**

- Base clock: 1193182 Hz (≈ 1.193 MHz)
- To generate interrupt every N ms, count = 1193182 / (1000 / N)
- For 10ms: count = 1193182 / 100 ≈ 11931

**PIT Programming:**

1. Write control byte to port 0x43
2. Write low byte of count to 0x40
3. Write high byte of count to 0x40

Control byte format:

```
Bit 7..6: Channel (00 = channel 0, only one we use)
Bit 5..4: Access mode (11 = both bytes)
Bit 3..1: Operating mode (010 = rate generator)
Bit 0:    BCD/Binary (0 = binary)

For timer: 0x36 = 0011 0110 (channel 0, both bytes, mode 2, binary)
```

---

## Implementation Step-by-Step

### Step 1: Create Interrupt Handler Assembly

We need interrupt handlers in assembly that:
1. Save all CPU registers
2. Call C handler function
3. Restore registers and return via `iret`

Create `kernel/interrupt.asm`:

```asm
global _timer_handler
global _keyboard_handler
global _idt_load

extern timer_interrupt
extern keyboard_interrupt

; Timer handler (IRQ 0 / Interrupt 32)
_timer_handler:
    pusha                  ; Save all general registers
    cld                    ; Clear direction flag
    call timer_interrupt   ; Call C function
    popa                   ; Restore registers
    iret                   ; Return from interrupt

; Keyboard handler (IRQ 1 / Interrupt 33)
_keyboard_handler:
    pusha
    cld
    call keyboard_interrupt
    popa
    iret

; Load IDT table
; Parameters (stack):
;   [esp+4] = IDT pointer (uint16_t limit | uint32_t base)
; Returns: Nothing (IDT loaded into IDTR)
_idt_load:
    mov eax, [esp+4]
    lidt [eax]
    ret
```

### Step 2: Create IDT & Descriptor Headers

Create `kernel/idt.h`:

```c
#ifndef IDT_H
#define IDT_H

#include <stdint.h>

typedef struct {
    uint16_t offset_lo;
    uint16_t selector;
    uint8_t  reserved;
    uint8_t  type_attr;
    uint16_t offset_hi;
} __attribute__((packed)) IDTEntry;

typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) IDTDescriptor;

void idt_init(void);
void idt_set_entry(int index, uint32_t handler, uint16_t selector, uint8_t flags);

#endif
```

### Step 3: Implement IDT

Create `kernel/idt.c`:

```c
#include "idt.h"

#define IDT_SIZE 256

static IDTEntry idt_table[IDT_SIZE];

extern void _idt_load(void* descriptor);

static void idt_set_entry(int index, uint32_t handler, uint16_t selector, uint8_t flags) {
    idt_table[index].offset_lo = handler & 0xFFFF;
    idt_table[index].offset_hi = (handler >> 16) & 0xFFFF;
    idt_table[index].selector = selector;
    idt_table[index].reserved = 0;
    idt_table[index].type_attr = flags;
}

void idt_init(void) {
    // Clear entire IDT
    for (int i = 0; i < IDT_SIZE; i++) {
        idt_set_entry(i, 0, 0, 0);
    }

    // Install handlers for IRQ 0 (timer) and IRQ 1 (keyboard)
    extern void _timer_handler(void);
    extern void _keyboard_handler(void);

    // Interrupt 32 = IRQ 0 (timer)
    // Interrupt 33 = IRQ 1 (keyboard)
    // 0x8E = Present, Ring 0, Trap Gate (32-bit)
    idt_set_entry(32, (uint32_t)_timer_handler, 0x08, 0x8E);
    idt_set_entry(33, (uint32_t)_keyboard_handler, 0x08, 0x8E);

    // Load IDT into CPU
    static IDTDescriptor descriptor;
    descriptor.limit = sizeof(idt_table) - 1;
    descriptor.base = (uint32_t)idt_table;

    _idt_load(&descriptor);
}
```

### Step 4: Create Timer Module

Create `kernel/timer.h`:

```c
#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void timer_init(void);
uint32_t timer_ticks(void);
void timer_sleep(uint32_t ms);

#endif
```

Create `kernel/timer.c`:

```c
#include "timer.h"
#include "io.h"
#include "vga.h"

static volatile uint32_t ticks = 0;

#define PIT_CHANNEL 0x40
#define PIT_CONTROL 0x43
#define PIT_FREQUENCY 1193182

void timer_init(void) {
    // Calculate count for 10ms interval
    // Frequency = 1193182 Hz, we want 100 interrupts per second (10ms each)
    uint32_t count = PIT_FREQUENCY / 100;  // 11931 ≈ 10ms

    // Port 0x43: Control byte = 0x36
    // Bit 7-6: 00 = channel 0
    // Bit 5-4: 11 = both bytes (low then high)
    // Bit 3-1: 010 = rate generator (mode 2)
    // Bit 0: 0 = binary
    outb(PIT_CONTROL, 0x36);

    // Port 0x40: data for channel 0
    // Write low byte first, then high byte
    outb(PIT_CHANNEL, count & 0xFF);
    outb(PIT_CHANNEL, (count >> 8) & 0xFF);
}

void timer_interrupt(void) {
    ticks++;

    // Acknowledge interrupt to PIC
    outb(0x20, 0x20);  // Master PIC: EOI (End Of Interrupt)
}

uint32_t timer_ticks(void) {
    return ticks;
}

void timer_sleep(uint32_t ms) {
    uint32_t wake_time = ticks + (ms / 10);
    while (ticks < wake_time);
}
```

### Step 5: Update Keyboard Driver (Interrupt-based)

Modify `kernel/keyboard.h`:

```c
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);
char keyboard_getchar(void);

#endif
```

Modify `kernel/keyboard.c`:

```c
#include "keyboard.h"
#include "io.h"

#define KEYBOARD_DATA 0x60
#define KEYBOARD_STATUS 0x64

static volatile char last_char = 0;
static volatile uint8_t has_char = 0;

static char scancode_to_ascii(uint8_t scancode) {
    static const char keymap[] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
        'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
        'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, '7', '8', '9', '-', '4', '5', '6', '+', '1',
        '2', '3', '0', '.', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    if (scancode < 128)
        return keymap[scancode];
    return 0;
}

void keyboard_init(void) {
    // Keyboard is ready to use, interrupts will handle input
}

void keyboard_interrupt(void) {
    // Read scancode
    uint8_t status = inb(KEYBOARD_STATUS);
    if (!(status & 1))
        return;  // No data available

    uint8_t scancode = inb(KEYBOARD_DATA);

    // Only handle key presses (bit 7 = 0 for press, 1 for release)
    if (!(scancode & 0x80)) {
        char c = scancode_to_ascii(scancode);
        if (c) {
            last_char = c;
            has_char = 1;
        }
    }

    // Acknowledge interrupt to PIC
    outb(0x20, 0x20);  // Master PIC: EOI
}

char keyboard_getchar(void) {
    while (!has_char);
    has_char = 0;
    return last_char;
}
```

### Step 6: Update PIC (Programmable Interrupt Controller)

The PIC controls which IRQs are enabled/disabled.

Create `kernel/pic.h`:

```c
#ifndef PIC_H
#define PIC_H

#include <stdint.h>

void pic_init(void);
void pic_disable_all(void);
void pic_enable_irq(uint8_t irq);
void pic_disable_irq(uint8_t irq);

#endif
```

Create `kernel/pic.c`:

```c
#include "pic.h"
#include "io.h"

#define PIC_MASTER_CMD    0x20
#define PIC_MASTER_DATA   0x21
#define PIC_SLAVE_CMD     0xA0
#define PIC_SLAVE_DATA    0xA1

#define ICW1_INIT         0x10
#define ICW1_ICW4         0x01
#define ICW4_8086         0x01

void pic_init(void) {
    // ICW1: Start initialization
    outb(PIC_MASTER_CMD, ICW1_INIT | ICW1_ICW4);
    outb(PIC_SLAVE_CMD, ICW1_INIT | ICW1_ICW4);

    // ICW2: Set interrupt vectors
    outb(PIC_MASTER_DATA, 32);   // IRQ 0 starts at interrupt 32
    outb(PIC_SLAVE_DATA, 40);    // IRQ 8 starts at interrupt 40

    // ICW3: Set cascading
    outb(PIC_MASTER_DATA, 0x04); // Slave on IRQ 2
    outb(PIC_SLAVE_DATA, 0x02);  // Connected to IRQ 2

    // ICW4: 8086 mode
    outb(PIC_MASTER_DATA, ICW4_8086);
    outb(PIC_SLAVE_DATA, ICW4_8086);

    // OCW1: Disable all interrupts initially
    pic_disable_all();
}

void pic_disable_all(void) {
    outb(PIC_MASTER_DATA, 0xFF);
    outb(PIC_SLAVE_DATA, 0xFF);
}

void pic_enable_irq(uint8_t irq) {
    if (irq < 8) {
        uint8_t mask = inb(PIC_MASTER_DATA);
        outb(PIC_MASTER_DATA, mask & ~(1 << irq));
    } else {
        uint8_t mask = inb(PIC_SLAVE_DATA);
        outb(PIC_SLAVE_DATA, mask & ~(1 << (irq - 8)));
    }
}

void pic_disable_irq(uint8_t irq) {
    if (irq < 8) {
        uint8_t mask = inb(PIC_MASTER_DATA);
        outb(PIC_MASTER_DATA, mask | (1 << irq));
    } else {
        uint8_t mask = inb(PIC_SLAVE_DATA);
        outb(PIC_SLAVE_DATA, mask | (1 << (irq - 8)));
    }
}
```

### Step 7: Update Kernel Main

Modify `kernel/kernel.c`:

```c
#include "vga.h"
#include "keyboard.h"
#include "timer.h"
#include "idt.h"
#include "pic.h"
#include "io.h"
#include "string.h"

#define INPUT_MAX 128

void kernel_main(void) {
    vga_clear();
    vga_print("=== OASIS Kernel: Initializing ===\n");

    // Initialize interrupt infrastructure
    vga_print("Initializing IDT...\n");
    idt_init();

    vga_print("Initializing PIC...\n");
    pic_init();

    vga_print("Initializing timer (IRQ 0)...\n");
    timer_init();
    pic_enable_irq(0);  // Enable timer

    vga_print("Initializing keyboard (IRQ 1)...\n");
    keyboard_init();
    pic_enable_irq(1);  // Enable keyboard

    // Enable interrupts (CPU-level)
    asm("sti");

    vga_print("=== OASIS Ready ===\n");
    vga_print("Interrupts enabled\n\n");

    char input[INPUT_MAX];
    int index = 0;

    vga_print("> ");

    while (1) {
        char c = keyboard_getchar();

        if (c == '\n') {
            input[index] = 0;
            vga_putc('\n');

            if (strcmp(input, "help") == 0) {
                vga_print("Commands:\n");
                vga_print("  help    - show this message\n");
                vga_print("  clear   - clear screen\n");
                vga_print("  uptime  - show system uptime\n");
            } else if (strcmp(input, "clear") == 0) {
                vga_clear();
            } else if (strcmp(input, "uptime") == 0) {
                uint32_t ticks = timer_ticks();
                uint32_t seconds = ticks / 100;
                uint32_t minutes = seconds / 60;
                uint32_t hours = minutes / 60;

                vga_print("Uptime: ");
                // Simple number printing (we'll add itoa later)
                if (hours > 0) {
                    vga_print("hours\n");
                } else {
                    vga_print("seconds: ");
                    vga_print("(add itoa)\n");
                }
            } else if (index != 0) {
                vga_print("unknown command\n");
            }

            index = 0;
            vga_print("> ");
            continue;
        }

        if (c == '\b') {
            if (index > 0) {
                index--;
                vga_putc('\b');
            }
            continue;
        }

        if (index < INPUT_MAX - 1) {
            input[index++] = c;
            vga_putc(c);
        }
    }
}
```

### Step 8: Update Makefile

Modify the Makefile to include new source files:

```makefile
CC = i686-elf-gcc
LD = i686-elf-ld
CFLAGS = -std=c99 -ffreestanding -fno-pie -Wall -Wextra
LDFLAGS = -m elf_i386 -T kernel/linker.ld -no-pie

.PHONY: all clean run iso

all: kernel.bin iso.iso

kernel.bin: boot/entry.o kernel/interrupt.o kernel/idt.o kernel/kernel.o kernel/vga.o kernel/keyboard.o kernel/io.o kernel/string.o kernel/timer.o kernel/pic.o
	$(LD) $(LDFLAGS) -o kernel.bin $^

boot/entry.o: boot/entry.asm
	nasm -f elf32 boot/entry.asm -o boot/entry.o

kernel/interrupt.o: kernel/interrupt.asm
	nasm -f elf32 kernel/interrupt.asm -o kernel/interrupt.o

kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) -c $< -o $@

iso.iso: kernel.bin
	mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/
	cp kernel/grub.cfg iso/boot/grub/
	grub-mkrescue -o iso.iso iso/

clean:
	rm -f boot/*.o kernel/*.o kernel.bin iso.iso
	rm -rf iso/boot

run: kernel.bin
	qemu-system-i386 -kernel kernel.bin -m 128M

.PHONY: debug
debug: kernel.bin
	qemu-system-i386 -kernel kernel.bin -m 128M -S -gdb tcp::1234
```

---

## Summary of Changes

### New Files Created

1. `kernel/interrupt.asm` – Interrupt handlers in assembly
2. `kernel/idt.h` / `kernel/idt.c` – IDT setup and management
3. `kernel/timer.h` / `kernel/timer.c` – PIT timer and tick tracking
4. `kernel/pic.h` / `kernel/pic.c` – PIC initialization and IRQ control

### Files Modified

1. `kernel/keyboard.h` / `kernel/keyboard.c` – Changed from polling to interrupt-driven
2. `kernel/kernel.c` – Added initialization sequence for interrupts
3. `Makefile` – Added new source files

### Architecture Change

```
Before (Day 3):                 After (Day 4):

Polling Loop                    Interrupt-Driven
├── keyboard_getchar()          ├── CPU runs main loop
│   └── while (!ready) spin     ├── [IRQ fires]
└── blocked                     ├── CPU calls handler
                                └── Handler updates buffer
                                
Result: Responsive                Result: Efficient + Responsive
```

---

## Testing & Validation

### Build

```bash
make clean
make
```

### Run

```bash
qemu-system-i386 -kernel kernel.bin -m 128M
```

### Expected Output

```
=== OASIS Kernel: Initializing ===
Initializing IDT...
Initializing PIC...
Initializing timer (IRQ 0)...
Initializing keyboard (IRQ 1)...
=== OASIS Ready ===
Interrupts enabled

> 
```

### Testing Interrupts

1. **Keyboard test:** Type characters (should be responsive)
2. **Timer test:** Run `uptime` command
3. **Stability test:** Type repeatedly for 30 seconds (should not crash)

---

## What Comes Next (Day 5)

Day 5 moves into memory management:

- Detect available physical memory (BIOS e820)
- Enable paging (virtual memory)
- Create page tables
- Implement physical page allocator

This is when Oasis gets **memory protection**.

---

## Key Takeaways

1. **Interrupts are fundamental** – Every real OS uses them
2. **PIC configuration is critical** – Must initialize before enabling IRQs
3. **Handler atomicity matters** – We'll improve this in Day 7 with proper locking
4. **Assembly bridges hardware and software** – Interrupt handlers must save/restore all state
5. **The shell now runs alongside interrupts** – Both the main loop and timer work concurrently

Your kernel is now interrupt-driven. This is huge progress.
