/*
 * gdt.c - Global Descriptor Table
 *
 * Setup GDT dengan segment untuk ring 0 dan ring 3 (user mode).
 *
 * Layout:
 *   0x00: NULL descriptor
 *   0x08: Kernel Code (ring 0, 4GB flat)
 *   0x10: Kernel Data (ring 0, 4GB flat)
 *   0x18: User Code   (ring 3, 4GB flat)
 *   0x20: User Data   (ring 3, 4GB flat)
 *   0x28: TSS         (Task State Segment, dipake CPU pas int dari ring 3)
 */

#include "gdt.h"
#include "vga.h"
#include "string.h"
#include "io.h"

/* Struktur GDT entry 8 byte */
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;

/* Struktur GDT pointer buat lgdt */
typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;

/* GDT entries */
static gdt_entry_t gdt_entries[6];
static gdt_ptr_t gdt_ptr;

/* TSS default */
static uint32_t tss[32] = {0};  /* 104 byte, cukup buat 26 uint32_t */

/* Helper: set satu GDT entry */
static void gdt_set_entry(int num, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t gran) {
    gdt_entries[num].base_low     = base & 0xFFFF;
    gdt_entries[num].base_mid     = (base >> 16) & 0xFF;
    gdt_entries[num].base_high    = (base >> 24) & 0xFF;
    gdt_entries[num].limit_low    = limit & 0xFFFF;
    gdt_entries[num].granularity  = (limit >> 16) & 0x0F;
    gdt_entries[num].granularity |= gran & 0xF0;
    gdt_entries[num].access       = access;
}

/* Setup TSS entry di GDT */
static void gdt_set_tss(int num) {
    uint32_t base = (uint32_t)&tss;
    uint32_t limit = sizeof(tss) - 1;

    gdt_set_entry(num, base, limit, 0x89, 0x40);  /* access=10001001, gran=01000000 */
    /* 0x89 = Present | ring 0 | TSS type (0x9 = available TSS) */
    /* 0x40 = byte granularity, 32-bit */
}

/* Setup isi TSS */
static void tss_set_stack(uint32_t kernel_ss, uint32_t kernel_esp) {
    /* TSS layout di x86:
     *   [0]  = previous task link (16 bit)
     *   [1]  = ESP0 (32 bit)
     *   [2]  = SS0 (16 bit)
     *   [3]  = ESP1
     *   [4]  = SS1
     *   [5]  = ESP2
     *   [6]  = SS2
     *   [7-24] = CR3, EIP, EFLAGS, EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
     *   [25] = ES, CS, SS, DS, FS, GS
     *   ...etc
     */
    tss[1] = kernel_esp;    /* ESP0 */
    tss[2] = kernel_ss;     /* SS0 */
}

void gdt_init(void) {
    vga_print("[*] Setting up GDT with user segments...\n");

    gdt_ptr.limit = sizeof(gdt_entries) - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;

    /* NULL descriptor */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* Kernel Code: base 0, limit 4GB, ring 0 */
    gdt_set_entry(1, 0, 0xFFFFF,
                  0x9A,  /* 10011010: Present | ring 0 | Code | Execute/Read */
                  0xCF); /* 11001111: 4KB gran | 32-bit */

    /* Kernel Data: base 0, limit 4GB, ring 0 */
    gdt_set_entry(2, 0, 0xFFFFF,
                  0x92,  /* 10010010: Present | ring 0 | Data | Read/Write */
                  0xCF); /* 11001111: 4KB gran | 32-bit */

    /* User Code: base 0, limit 4GB, ring 3 */
    gdt_set_entry(3, 0, 0xFFFFF,
                  0xFA,  /* 11111010: Present | ring 3 | Code | Execute/Read */
                  0xCF); /* 11001111: 4KB gran | 32-bit */

    /* User Data: base 0, limit 4GB, ring 3 */
    gdt_set_entry(4, 0, 0xFFFFF,
                  0xF2,  /* 11110010: Present | ring 3 | Data | Read/Write */
                  0xCF); /* 11001111: 4KB gran | 32-bit */

    /* TSS segment */
    gdt_set_tss(5);

    /* Setup TSS awal: kernel stack untuk saat user task int 0x80 */
    /* ESP0 = stack_top dari entry.asm (0x29E840), SS0 = 0x10 */
    extern uint32_t stack_top;  /* dari entry.asm */
    tss_set_stack(0x10, (uint32_t)&stack_top);

    /* Load GDT pake lgdt */
    asm volatile(
        "lgdtl %0\n"
        /* Reload segmen selector biar GDT baru kepake */
        "movl $0x10, %%eax\n"   /* Kernel Data */
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%fs\n"
        "movw %%ax, %%gs\n"
        /* Far jump buat reload CS */
        "ljmp $0x08, $.reload\n"
        ".reload:\n"
        : : "m"(gdt_ptr) : "eax", "memory"
    );

    /* Load TSS: pake LTR (Load Task Register) */
    /* TSS selector = 0x28 (index 5, RPL 0) */
    uint16_t tss_sel = 5 * 8;
    asm volatile("ltrw %0" : : "r"(tss_sel));

    vga_print("[+] GDT with user segments loaded\n");
    vga_print("    0x08: Kernel Code (ring 0)\n");
    vga_print("    0x10: Kernel Data (ring 0)\n");
    vga_print("    0x18: User Code (ring 3)\n");
    vga_print("    0x20: User Data (ring 3)\n");
    vga_print("    0x28: TSS\n");
}
