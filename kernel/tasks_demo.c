#include "tasks_demo.h"
#include "vga.h"
#include "timer.h"
#include "string.h"

volatile int counter_a = 0;
volatile int counter_b = 0;

void task_idle(void) {
    char buf[16];
    while (1) {
        counter_a++;
        if(counter_a % 10000000 == 0) {
            vga_print("[Task 1] Executing..");
            itoa(counter_a, buf, 10);
            vga_print(buf);
            vga_print("\n");
        }
    }
}

void task_worker(void) {
    char buf[16];
    while (1) {
        counter_b++;
        if(counter_b % 10000000 == 0) {
            vga_print("[Task 2] Working.. counter=");
            itoa(counter_b, buf, 10);
            vga_print(buf);
            vga_print("\n");
        }
    }
}