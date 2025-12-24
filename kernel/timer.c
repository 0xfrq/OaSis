#include "timer.h"
#include "pic.h"
#include "io.h"
#include "task.h"

#define PIT_CHANNEL_0 0x40
#define PIT_CONTROL   0x43
#define PIT_FREQUENCY 1193182

static volatile uint32_t ticks = 0;

void timer_init(uint32_t frequency) {
    uint32_t divisor = PIT_FREQUENCY / frequency;

    // Send control byte to PIT
    // 0x36 = channel 0, both bytes, mode 2 (rate generator), binary
    outb(PIT_CONTROL, 0x36);

    // Send divisor (low byte first, then high byte)
    outb(PIT_CHANNEL_0, divisor & 0xFF);
    outb(PIT_CHANNEL_0, (divisor >> 8) & 0xFF);

    // Enable IRQ 0 on the PIC
    pic_enable_irq(0);
}

void timer_interrupt_handler(void) {
    ticks++;

    task_switch();
}

uint32_t timer_get_ticks(void) {
    return ticks;
}

void timer_sleep(uint32_t milliseconds) {
    uint32_t target = ticks + (milliseconds / 10);  // 10ms per tick at 100Hz
    while (ticks < target);
}
