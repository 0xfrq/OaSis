#include "idt.h"
#include "vga.h"
#include "string.h"

static IDTEntry idt[IDT_ENTRIES];
static IDTPointer idt_ptr;

extern void load_idt(void*);

extern void isr_0(void);
extern void isr_1(void);
extern void isr_2(void);
extern void isr_3(void);
extern void isr_4(void);
extern void isr_5(void);
extern void isr_6(void);
extern void isr_7(void);
extern void isr_8(void);
extern void isr_9(void);
extern void isr_10(void);
extern void isr_11(void);
extern void isr_12(void);
extern void isr_13(void);
extern void isr_14(void);
extern void isr_15(void);
extern void isr_16(void);
extern void isr_17(void);
extern void isr_18(void);
extern void isr_19(void);
extern void isr_20(void);
extern void isr_21(void);
extern void isr_22(void);
extern void isr_23(void);
extern void isr_24(void);
extern void isr_25(void);
extern void isr_26(void);
extern void isr_27(void);
extern void isr_28(void);
extern void isr_29(void);
extern void isr_30(void);
extern void isr_31(void);

extern void irq_0(void);
extern void irq_1(void);
extern void irq_2(void);
extern void irq_3(void);
extern void irq_4(void);
extern void irq_5(void);
extern void irq_6(void);
extern void irq_7(void);
extern void irq_8(void);
extern void irq_9(void);
extern void irq_10(void);
extern void irq_11(void);
extern void irq_12(void);
extern void irq_13(void);
extern void irq_14(void);
extern void irq_15(void);

void idt_set_entry(int num, uint32_t handler, uint16_t selector, uint8_t type_attr) {
    idt[num].offset_lo = handler & 0xFFFF;
    idt[num].offset_hi = (handler >> 16) & 0xFFFF;
    idt[num].selector = selector;
    idt[num].type_attr = type_attr;
    idt[num].reserved = 0;
}

void idt_init(void) {
    idt_ptr.limit = (sizeof(IDTEntry) * IDT_ENTRIES) - 1;
    idt_ptr.base = (uint32_t)&idt;

    idt_set_entry(0, (uint32_t)isr_0, 0x08, 0x8E);
    idt_set_entry(1, (uint32_t)isr_1, 0x08, 0x8E);
    idt_set_entry(2, (uint32_t)isr_2, 0x08, 0x8E);
    idt_set_entry(3, (uint32_t)isr_3, 0x08, 0x8E);
    idt_set_entry(4, (uint32_t)isr_4, 0x08, 0x8E);
    idt_set_entry(5, (uint32_t)isr_5, 0x08, 0x8E);
    idt_set_entry(6, (uint32_t)isr_6, 0x08, 0x8E);
    idt_set_entry(7, (uint32_t)isr_7, 0x08, 0x8E);
    idt_set_entry(8, (uint32_t)isr_8, 0x08, 0x8E);
    idt_set_entry(9, (uint32_t)isr_9, 0x08, 0x8E);
    idt_set_entry(10, (uint32_t)isr_10, 0x08, 0x8E);
    idt_set_entry(11, (uint32_t)isr_11, 0x08, 0x8E);
    idt_set_entry(12, (uint32_t)isr_12, 0x08, 0x8E);
    idt_set_entry(13, (uint32_t)isr_13, 0x08, 0x8E);
    idt_set_entry(14, (uint32_t)isr_14, 0x08, 0x8E);
    idt_set_entry(15, (uint32_t)isr_15, 0x08, 0x8E);
    idt_set_entry(16, (uint32_t)isr_16, 0x08, 0x8E);
    idt_set_entry(17, (uint32_t)isr_17, 0x08, 0x8E);
    idt_set_entry(18, (uint32_t)isr_18, 0x08, 0x8E);
    idt_set_entry(19, (uint32_t)isr_19, 0x08, 0x8E);
    idt_set_entry(20, (uint32_t)isr_20, 0x08, 0x8E);
    idt_set_entry(21, (uint32_t)isr_21, 0x08, 0x8E);
    idt_set_entry(22, (uint32_t)isr_22, 0x08, 0x8E);
    idt_set_entry(23, (uint32_t)isr_23, 0x08, 0x8E);
    idt_set_entry(24, (uint32_t)isr_24, 0x08, 0x8E);
    idt_set_entry(25, (uint32_t)isr_25, 0x08, 0x8E);
    idt_set_entry(26, (uint32_t)isr_26, 0x08, 0x8E);
    idt_set_entry(27, (uint32_t)isr_27, 0x08, 0x8E);
    idt_set_entry(28, (uint32_t)isr_28, 0x08, 0x8E);
    idt_set_entry(29, (uint32_t)isr_29, 0x08, 0x8E);
    idt_set_entry(30, (uint32_t)isr_30, 0x08, 0x8E);
    idt_set_entry(31, (uint32_t)isr_31, 0x08, 0x8E);

    idt_set_entry(32, (uint32_t)irq_0, 0x08, 0x8E);
    idt_set_entry(33, (uint32_t)irq_1, 0x08, 0x8E);
    idt_set_entry(34, (uint32_t)irq_2, 0x08, 0x8E);
    idt_set_entry(35, (uint32_t)irq_3, 0x08, 0x8E);
    idt_set_entry(36, (uint32_t)irq_4, 0x08, 0x8E);
    idt_set_entry(37, (uint32_t)irq_5, 0x08, 0x8E);
    idt_set_entry(38, (uint32_t)irq_6, 0x08, 0x8E);
    idt_set_entry(39, (uint32_t)irq_7, 0x08, 0x8E);
    idt_set_entry(40, (uint32_t)irq_8, 0x08, 0x8E);
    idt_set_entry(41, (uint32_t)irq_9, 0x08, 0x8E);
    idt_set_entry(42, (uint32_t)irq_10, 0x08, 0x8E);
    idt_set_entry(43, (uint32_t)irq_11, 0x08, 0x8E);
    idt_set_entry(44, (uint32_t)irq_12, 0x08, 0x8E);
    idt_set_entry(45, (uint32_t)irq_13, 0x08, 0x8E);
    idt_set_entry(46, (uint32_t)irq_14, 0x08, 0x8E);
    idt_set_entry(47, (uint32_t)irq_15, 0x08, 0x8E);

    load_idt(&idt_ptr);
}

void interrupt_handler(int int_num, int err_code) {
    if (int_num < 32) {
        vga_print("Exception ");
        char buf[16];
        itoa(int_num, buf, 10);
        vga_print(buf);
        vga_print("\n");
    }
}
