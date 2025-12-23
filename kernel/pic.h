#ifndef PIC_H
#define PIC_H

#include <stdint.h>

void pic_init(void);
void pic_enable_irq(int irq);
void pic_disable_irq(int irq);

#endif
