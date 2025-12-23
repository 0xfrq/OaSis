# Oasis OS: 14-Day Complete Roadmap

## Executive Summary

This roadmap outlines a methodical progression from bare metal boot to a fully functional preemptive multitasking kernel. Each day builds on the previous one with clear, testable goals.

---

## Days 1-3: Foundation (Already Complete ✓)

### Day 1 – Prove Kernel Execution
- Bootloader (GRUB)
- Entry point (assembly)
- Basic kernel initialization in C
- Proof: Kernel runs on bare metal

### Day 2 – Build Console I/O
- VGA text mode driver
- Keyboard polling driver
- Screen control (clear, putc, print)
- Proof: Interactive text output/input

### Day 3 – Create Shell
- Input buffering (line-based)
- Command parsing
- Screen scrolling
- Basic shell loop (help, clear)
- Proof: Real CLI works

---

## Days 4-7: Hardware Control

### Day 4 – Interrupt Infrastructure ⭐ (TODAY)
- PIT (Programmable Interval Timer) for time
- IDT (Interrupt Descriptor Table) setup
- IRQ 0 (timer) handler
- Replace keyboard polling with IRQ 1
- Uptime tracking
- Proof: Interrupts work reliably

### Day 5 – Memory Management Basics
- Physical memory detection (BIOS e820)
- Paging setup (4KB pages)
- Virtual address space layout
- Page allocator (simple bump)
- Proof: Memory isolation works

### Day 6 – Process/Task Primitives
- Process structure (TCB)
- Context switching (stack, registers)
- Simple task creation
- Basic scheduler (round-robin)
- Proof: Two tasks run concurrently

### Day 7 – Interrupts & Safety
- Task switching in timer interrupt
- Stack isolation per task
- Kernel stack per task
- Atomic operations
- Proof: Multitasking is stable

---

## Days 8-10: System Services

### Day 8 – System Call Interface
- Syscall convention (int 0x80)
- Syscall dispatcher
- Basic syscalls: exit, getpid, write, sleep
- User mode entry
- Proof: Userspace programs run

### Day 9 – Process Management
- Fork (create child process)
- Exec (load and run program)
- Wait (parent synchronization)
- Process hierarchy (parent/child)
- Proof: Complex process trees work

### Day 10 – I/O Subsystem
- File descriptor table
- Open, read, write, close
- Standard I/O (stdin, stdout, stderr)
- Pipe basics
- Proof: Programs can read/write data

---

## Days 11-12: Storage & Filesystem

### Day 11 – Block Device Abstraction
- ATA disk driver (read/write)
- Disk I/O queueing
- Block cache
- Proof: Reliable disk I/O

### Day 12 – Simple Filesystem
- Filesystem design (superblock, inode, block)
- Read-only mount
- Directory traversal
- File reading from disk
- Proof: Files persist and are readable

---

## Days 13-14: Polish & Testing

### Day 13 – Shell Enhancements
- Program loading from disk
- Argument passing
- Output redirection (>)
- Background jobs (&)
- Proof: Shell is usable

### Day 14 – Testing & Robustness
- Stress tests
- Error recovery
- Performance optimization
- Documentation
- Proof: Oasis is production-ready (for education)

---

## Key Principles (Apply to All Days)

1. **One feature per day** – Focused, testable work
2. **Test every step** – Build and boot after each change
3. **Keep it simple** – Avoid over-engineering
4. **Document code** – Future you will thank present you
5. **Backwards compatible** – Never break Day 1

---

## Architecture Summary

```
┌─────────────────────────────────────┐
│        Userspace Programs           │  Day 9, 13
├─────────────────────────────────────┤
│     System Call Interface (int 0x80) │  Day 8
├─────────────────────────────────────┤
│  Process Manager / Scheduler         │  Days 6-7, 9
│  Memory Manager / Paging             │  Day 5
│  I/O Subsystem / File Descriptors    │  Day 10
│  Filesystem & Block Device           │  Days 11-12
│  Interrupt & Timer Handling          │  Day 4
├─────────────────────────────────────┤
│  Hardware (CPU, Memory, Disk, I/O)   │
└─────────────────────────────────────┘
```

---

## Success Metrics

- **Day 7:** Multitasking OS boots and switches tasks
- **Day 10:** Programs can read/write data
- **Day 12:** Files persist on disk and load correctly
- **Day 14:** Complex shell programs work without crashes

Anything less is a work in progress.
