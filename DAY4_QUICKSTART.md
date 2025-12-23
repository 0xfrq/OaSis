# Day 4 Implementation Summary & Quick Start

## What Day 4 Accomplishes

Day 4 transforms Oasis from a **polling-based OS** to an **interrupt-driven OS**. This is the fundamental shift that makes a system "real."

### Before Day 4 (Polling)
- Keyboard checked repeatedly in a loop
- CPU wastes cycles even when nothing happens
- Timer doesn't exist

### After Day 4 (Interrupts)
- Keyboard input triggers IRQ 1 automatically
- CPU runs main loop uninterrupted
- PIT timer generates IRQ 0 every 10ms
- System tracks uptime

---

## Files to Create

Create these 4 new files in the `kernel/` directory:

### 1. `kernel/interrupt.asm`
Assembly handlers for CPU interrupts. These save CPU state, call C handlers, then restore state.

### 2. `kernel/idt.h` and `kernel/idt.c`
Interrupt Descriptor Table management. Maps interrupt numbers to handler functions.

### 3. `kernel/timer.h` and `kernel/timer.c`
PIT (Programmable Interval Timer) initialization and tick tracking.

### 4. `kernel/pic.h` and `kernel/pic.c`
PIC (Programmable Interrupt Controller) setup to enable/disable individual IRQs.

---

## Files to Modify

### 1. `kernel/keyboard.c`
Change from polling-based to interrupt-driven. Handler fires on IRQ 1.

### 2. `kernel/kernel.c`
Add initialization sequence: IDT → PIC → Timer → Keyboard, then `sti` to enable interrupts.

### 3. `Makefile`
Add compilation rules for new `.asm` and `.c` files.

---

## Implementation Sequence

**Follow this order exactly:**

1. Create `kernel/interrupt.asm`
2. Create `kernel/idt.h` and `kernel/idt.c`
3. Create `kernel/timer.h` and `kernel/timer.c`
4. Create `kernel/pic.h` and `kernel/pic.c`
5. Modify `kernel/keyboard.c` (interrupt-driven version)
6. Modify `kernel/kernel.c` (initialization + main loop)
7. Modify `Makefile` (add new file rules)
8. Run `make clean && make && qemu-system-i386 -kernel kernel.bin -m 128M`

---

## Key Concepts

### IDT (Interrupt Descriptor Table)
Maps interrupt numbers (0-255) to handler functions. Entry format:

```c
struct IDTEntry {
    uint16_t offset_lo;    // Handler address (low 16 bits)
    uint16_t selector;     // Code segment (0x08 for kernel)
    uint8_t  reserved;     // 0
    uint8_t  type_attr;    // 0x8E (trap gate, present, ring 0)
    uint16_t offset_hi;    // Handler address (high 16 bits)
};
```

### PIT (Programmable Interval Timer)
Generates clock interrupts at regular intervals.

```
Port 0x43: Control register
Port 0x40: Data (channel 0)

Control byte 0x36:
  - Channel 0
  - Access both bytes
  - Mode 2 (rate generator)
  - Binary mode

Count = 1193182 / (interrupts per second)
       = 1193182 / 100  (for 10ms = 100 Hz)
       ≈ 11931
```

### PIC (Programmable Interrupt Controller)
Manages hardware interrupt routing.

```
Port 0x20:  Master PIC command
Port 0x21:  Master PIC data (interrupt mask)
Port 0xA0:  Slave PIC command
Port 0xA1:  Slave PIC data (interrupt mask)

IRQ 0:      Timer
IRQ 1:      Keyboard
IRQ 8-15:   Secondary devices (on slave)
```

### Interrupt Flow

```
1. Hardware event (key press, timer tick)
2. CPU saves state (registers, instruction pointer)
3. CPU jumps to handler (via IDT)
4. Handler runs (in kernel mode, interrupts disabled)
5. Handler returns via iret (CPU restores state)
6. Normal code continues
```

---

## Testing Checklist

After building Day 4:

- [ ] Kernel boots without errors
- [ ] "=== OASIS Ready ===" message appears
- [ ] "Interrupts enabled" message appears
- [ ] Typing works (keyboard responds)
- [ ] `uptime` command works (or shows placeholder)
- [ ] System doesn't crash for 30+ seconds of typing
- [ ] `help` command lists all available commands
- [ ] `clear` command clears screen

---

## Troubleshooting

### Kernel won't boot
- Check `kernel.c` for syntax errors in initialization
- Verify `interrupt.asm` compiles (check NASM syntax)

### No output, but no crash
- Kernel is likely waiting for keyboard input
- This is normal! Try typing.

### Keyboard not working
- Check `pic_enable_irq(1)` is called before `sti`
- Verify `keyboard_interrupt()` is being called
- Check scancode-to-ASCII mapping

### Random crashes
- Check stack alignment in `interrupt.asm`
- Verify `pusha`/`popa` balance
- Check for buffer overflows in input buffer

---

## What Comes After Day 4

**Day 5: Memory Management**
- Physical memory detection (BIOS e820)
- Paging (virtual memory)
- Page table setup
- Memory protection

At this point, Oasis becomes truly sophisticated:
- Multiple tasks will have isolated memory spaces
- No task can crash the system via memory corruption
- The kernel controls all hardware directly

---

## Commands to Know

```bash
# Clean build
make clean && make

# Run in QEMU
qemu-system-i386 -kernel kernel.bin -m 128M

# Debug with GDB
qemu-system-i386 -kernel kernel.bin -m 128M -S -gdb tcp::1234
# In another terminal: gdb
# (gdb) target remote :1234
# (gdb) break kernel_main
# (gdb) continue
```

---

## Milestone Achieved

🎉 **Oasis is now interrupt-driven.**

This is a critical milestone. You've gone from:
- Simple polling (Day 3)
- To real-time hardware handling (Day 4)

Your OS is no longer just "a program that runs on bare metal." It's a **real operating system** that responds to hardware events in real-time.

From here on, everything builds on this foundation:
- Multitasking (Day 6-7)
- System calls (Day 8)
- Filesystem (Day 11-12)

You're on track to build a production-quality educational OS.

---

## Detailed File Listings

All file contents are provided in `day4.md`. Copy and paste each file exactly as shown.

Key files:
- Longest: `kernel/timer.c` (~40 lines)
- Most complex: `kernel/idt.c` (IDT initialization)
- Most critical: `kernel/interrupt.asm` (handles all CPU/hardware communication)

Total new code: ~600 lines across 4 files
Modified code: ~3 files

This is substantial but focused work. Take your time, test after each file, and debug methodically.
