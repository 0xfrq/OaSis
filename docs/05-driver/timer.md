# timer driver

dokumentasi ini membahas bagaimana OaSis mengatur waktu menggunakan PIT (Programmable Interval Timer).

## daftar isi

- [overview](#overview)
- [pit (programmable interval timer)](#pit-programmable-interval-timer)
- [konfigurasi timer](#konfigurasi-timer)
- [interrupt handling](#interrupt-handling)
- [api reference](#api-reference)

---

## overview

**timer driver** di OaSis menggunakan PIT untuk generate periodic interrupt.

### fungsi

- generate interrupt setiap 10 ms (100 Hz)
- tracking system uptime
- trigger scheduler untuk preemptive multitasking
- support sleep/delay functions

### kenapa butuh timer?

- **preemptive scheduling**: task bisa di-interrupt otomatis
- **time tracking**: tahu berapa lama system sudah jalan
- **delay/sleep**: task bisa sleep untuk durasi tertentu
- **timeout**: detect operasi yang terlalu lama

## pit (programmable interval timer)

PIT adalah chip yang bisa generate periodic interrupt.

### karakteristik

- **base frequency**: 1.193182 MHz
- **3 channels**:
  - channel 0: system timer (yang kita menggunakan)
  - channel 1: DRAM refresh (tidak digunakan)
  - channel 2: PC speaker (tidak digunakan)

### port I/O

```
0x40: channel 0 data
0x41: channel 1 data
0x42: channel 2 data
0x43: command register
```

## konfigurasi timer

### set frequency

untuk set frequency, kita hitung divider:

```
divider = base_frequency / desired_frequency
```

**contoh:** 100 Hz (10 ms interval)
```
divider = 1193182 / 100 = 11932
```

### implementasi

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

## interrupt handling

timer menggunakan **IRQ 0** (interrupt 32 setelah remapping).

### flow

```
PIT generate interrupt setiap 10 ms
  ↓
IRQ 0 triggered
  ↓
cpu lompat ke int 32
  ↓
timer_handler() dipanggil
  ↓
increment tick counter
  ↓
call scheduler (preemptive)
  ↓
send EOI ke pic
  ↓
return dari interrupt
```

### implementasi

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

## api reference

### inisialisasi

```c
void timer_init(uint32_t frequency);
```

inisialisasi timer dengan frequency tertentu (dalam Hz).

**parameter:**
- `frequency`: interrupt frequency (biasanya 100 Hz = 10 ms)

**contoh:**
```c
timer_init(100);  // 100 Hz = interrupt setiap 10 ms
```

### get tick

```c
uint32_t timer_get_tick(void);
```

mendapatkan jumlah tick sejak boot.

**return:** jumlah tick

**catatan:** 1 tick = 10 ms (kalau frequency 100 Hz)

### get uptime (seconds)

```c
uint32_t timer_get_uptime_seconds(void);
```

mendapatkan uptime dalam detik.

**return:** uptime dalam detik

### get uptime (milliseconds)

```c
uint32_t timer_get_uptime_ms(void);
```

mendapatkan uptime dalam millisecond.

**return:** uptime dalam ms

### delay

```c
void timer_delay(uint32_t ms);
```

delay (blocking) untuk beberapa millisecond.

**parameter:**
- `ms`: jumlah millisecond untuk delay

**contoh:**
```c
timer_delay(1000);  // delay 1 second
```

### sleep

```c
void timer_sleep(uint32_t ticks);
```

sleep (non-blocking, task yield) untuk beberapa tick.

**parameter:**
- `ticks`: jumlah tick untuk sleep (1 tick = 10 ms)

**contoh:**
```c
timer_sleep(100);  // sleep 1 second (100 * 10ms)
```

---

## contoh penggunaan

### contoh 1: uptime display

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

### contoh 2: periodic task

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

### contoh 3: timeout

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

## troubleshooting

### timer tidak jalan

- cek IRQ 0 enabled di PIC
- cek timer_handler() registered di IDT
- cek PIT initialization

### interrupt terlalu sering/jarang

- cek frequency parameter
- cek divisor calculation
- cek PIT command byte

### scheduler tidak dipanggil

- cek timer_handler() call schedule()
- cek current_task != NULL
- cek task system initialized

---

**kembali ke:** [driver →](readme.md)
