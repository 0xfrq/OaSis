#include "timer.h"
#include "pic.h"
#include "io.h"
#include "task.h"

#define PIT_CHANNEL_0 0x40
#define PIT_CONTROL   0x43
#define PIT_FREQUENCY 1193182

static volatile uint32_t ticks = 0;

// inisialisasi timer PIT
void timer_init(uint32_t frequency) {
    uint32_t divisor = PIT_FREQUENCY / frequency;

    // kirim control byte ke PIT
    // 0x36 = channel 0, dua byte, mode 2 (rate generator), binary
    outb(PIT_CONTROL, 0x36);

    // kirim divisor (byte rendah dulu, baru byte tinggi)
    outb(PIT_CHANNEL_0, divisor & 0xFF);
    outb(PIT_CHANNEL_0, (divisor >> 8) & 0xFF);

    // aktifkan IRQ 0 di PIC
    pic_enable_irq(0);
}

// handler interrupt timer, tambah tick terus switch task
void timer_interrupt_handler(void) {
    ticks++;

    task_switch();
}

// ambil jumlah tick sekarang
uint32_t timer_get_ticks(void) {
    return ticks;
}

// tidur selama X milidetik
void timer_sleep(uint32_t milliseconds) {
    uint32_t target = ticks + (milliseconds / 10);  // 10ms per tick di 100Hz
    while (ticks < target);
}
