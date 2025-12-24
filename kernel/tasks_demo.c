#include "tasks_demo.h"
#include "vga.h"
#include "timer.h"
#include "string.h"

volatile int counter_a = 0;
volatile int counter_b = 0;

void task_idle(void) {
    while (1) {
        counter_a++;
        if(counter_a % 10000000 == 0) {

        }
    }
}

void task_worker(void) {
    while (1) {
        counter_b++;
        if(counter_b % 10000000 == 0) {

        }
    }
}