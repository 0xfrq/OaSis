# Timer driver

this page explains bagaimana OaSis set waktu using PIT (Programmable Interval Timer).

## Contents

- [overview](#overview)
- [pit (programmable interval timer)](#pit-programmable-interval-timer)
- [konfigurasi timer](#konfigurasi-timer)
- [interrupt handling](#interrupt-handling)
- [api reference](#api-reference)

---

## Overview

**timer driver** uses the PIT to generate periodic interrupts.

### Function

- generate interrupt each 10 ms (100 Hz)
- tracking system uptime
- trigger scheduler for preemptive multitasking
- support sleep/delay functions

### Why butuh timer?

- **preemptive scheduling**: task can di-interrupt otomatis
- **time tracking**: tahu berapa old system already jalan
- **delay/sleep**: tasks can sleep for a specific duration
- **timeout**: detect operations that take too long

## Pit (programmable interval timer)

PIT is chip that can generate periodic interrupt.

### Karakteristik

- **base frequency**: 1.193182 MHz
- **3 channels**:
  - channel 0: system timer (that kita using)
  - channel 1: DRAM refresh (not used)
  - channel 2: PC speaker (not used)

### Port I/O

```text
0x40: channel 0 data
0x41: channel 1 data
0x42: channel 2 data
0x43: command register
```

## Konfigurasi timer

### Set frequency

for set frequency, kita hitung divider:

```text
divider = base_frequency / desired_frequency
```

**example:** 100 Hz (10 ms interval)
```text
divider = 1193182 / 100 = 11932
```

### Implementasi

```c
void timer_init(uint32_t frequency) {
    // calculate divider
    uint32_t divisor = 1193182 / frequency;
    
    // send command byte
    outb(0x43, 0x36);  // channel 0, lobyte/hibyte, rate generator
    
    // send divisor (low byte first, then high byte)
    outb(0x40, divisor & 0xFF);          // low byte
    outb(0x40, (divisor >> 8) & 0xFF);   // high byte
    
    // register interrupt handler
    register_interrupt_handler(32, timer_handler);  // IRQ 0 = int 32
}
```

## Interrupt handling

timer using **IRQ 0** (interrupt 32 after remapping).

### Flow

```text
PIT generates an interrupt every 10 ms
  ↓
IRQ 0 triggered
  ↓
CPU jumps to interrupt 32
  ↓
timer_handler() is called
  ↓
increment tick counter
  ↓
call scheduler (preemptive)
  ↓
send EOI to the PIC
  ↓
return from the interrupt
```

### Implementasi

```c
static uint32_t tick = 0;

void timer_handler(void) {
    tick++;
    
    // wake up sleeping tasks
    task_wake_sleeping(tick);
    
    // call scheduler (preemptive)
    if (current_task != NULL) {
        schedule();
    }
    
    // send EOI to PIC
    pic_send_eoi(0);  // IRQ 0
}
```

## Api reference

### Initialization

```c
void timer_init(uint32_t frequency);
```

initialization timer with a specific frequency (in Hz).

**parameter:**
- `frequency`: interrupt frequency (usually 100 Hz = 10 ms)

**example:**
```c
timer_init(100);  // 100 Hz = interrupt every 10 ms
```

### Get tick

```c
uint32_t timer_get_tick(void);
```

get the number of ticks since boot.

**return:** number tick

**note:** 1 tick = 10 ms (if frequency 100 Hz)

### Get uptime (seconds)

```c
uint32_t timer_get_uptime_seconds(void);
```

get uptime in seconds.

**return:** uptime in detik

### Get uptime (milliseconds)

```c
uint32_t timer_get_uptime_ms(void);
```

get uptime in milliseconds.

**return:** uptime in ms

### Delay

```c
void timer_delay(uint32_t ms);
```

delay (blocking) for beberapa millisecond.

**parameter:**
- `ms`: number millisecond for delay

**example:**
```c
timer_delay(1000);  // delay 1 second
```

### Sleep

```c
void timer_sleep(uint32_t ticks);
```

sleep (non-blocking, task yield) for beberapa tick.

**parameter:**
- `ticks`: number tick for sleep (1 tick = 10 ms)

**example:**
```c
timer_sleep(100);  // sleep 1 second (100 * 10ms)
```

---

## Usage example

### Example 1: uptime display

```c
void show_uptime(void) {
    uint32_t seconds = timer_get_uptime_seconds();
    uint32_t minutes = seconds / 60;
    uint32_t hours = minutes / 60;
    
    seconds %= 60;
    minutes %= 60;
    
    char buf[50];
    sprintf(buf, "uptime: %d:%02d:%02d\n", hours, minutes, seconds);
    vga_puts(buf);
}
```

### Example 2: periodic task

```c
void periodic_task(void) {
    uint32_t last_tick = timer_get_tick();
    
    while (1) {
        // wait 1 second (100 ticks)
        while (timer_get_tick() - last_tick < 100) {
            task_yield();
        }
        last_tick = timer_get_tick();
        
        // do something every second
        vga_puts("tick\n");
    }
}
```

### Example 3: timeout

```c
int wait_with_timeout(int (*condition)(void), uint32_t timeout_ms) {
    uint32_t start = timer_get_uptime_ms();
    
    while (!condition()) {
        if (timer_get_uptime_ms() - start >= timeout_ms) {
            return -1;  // timeout
        }
        task_yield();
    }
    
    return 0;  // success
}
```

---

## Troubleshooting

### Timer not jalan

- check that IRQ 0 is enabled in the PIC
- check that timer_handler() is registered in the IDT
- cek PIT initialization

### Interrupts too frequent or too rare

- cek frequency parameter
- cek divisor calculation
- cek PIT command byte

### Scheduler not called

- cek timer_handler() call schedule()
- cek current_task != NULL
- cek task system initialized

---

**back to:** [driver →](readme.md)
