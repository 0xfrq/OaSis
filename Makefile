# ============================================
# OaSis OS - Makefile
# Sistem operasi edukasi sederhana
# ============================================

CC = gcc
CFLAGS = -m32 -nostdlib -fno-builtin -fno-stack-protector -ffreestanding -fno-pie -fno-pic -Wall -Wextra -Iinclude
AS = nasm
ASFLAGS = -f elf32
LD = ld
LDFLAGS = -m elf_i386 -T src/boot/linker.ld -no-pie

# Sumber kode C - dibagi per modul
SOURCES_CORE = src/kernel/core/kernel.c \
               src/kernel/core/memory.c \
               src/kernel/core/paging.c \
               src/kernel/core/pmm.c \
               src/kernel/core/vga.c \
               src/kernel/core/gdt.c

SOURCES_DRIVERS = src/kernel/drivers/ata.c \
                  src/kernel/drivers/block.c \
                  src/kernel/drivers/idt.c \
                  src/kernel/drivers/io.c \
                  src/kernel/drivers/keyboard.c \
                  src/kernel/drivers/pic.c \
                  src/kernel/drivers/timer.c

SOURCES_FS = src/kernel/fs/fd.c \
             src/kernel/fs/vfs.c

SOURCES_LIB = src/kernel/lib/string.c \
              src/kernel/lib/lexer.c \
              src/kernel/lib/parser.c \
              src/kernel/lib/codegen.c \
              src/kernel/lib/klibc.c \
              src/kernel/lib/heap.c \
              src/kernel/lib/log.c

SOURCES_SYSCALL = src/kernel/syscall/syscall.c

SOURCES_TASKS = src/kernel/tasks/task.c \
                src/kernel/tasks/tasks_10.c \
                src/kernel/tasks/tasks_11.c \
                src/kernel/tasks/tasks_demo.c \
                src/kernel/tasks/tasks_io.c \
                src/kernel/tasks/task_user.c

SOURCES_APPS = src/kernel/apps/editor.c \
               src/kernel/apps/asm.c

SOURCES_C = $(SOURCES_CORE) $(SOURCES_DRIVERS) $(SOURCES_FS) $(SOURCES_LIB) $(SOURCES_SYSCALL) $(SOURCES_TASKS) $(SOURCES_APPS)

# Sumber kode assembly
SOURCES_ASM = src/boot/entry.asm \
              src/kernel/syscall/interrupt.asm

OBJECTS = $(SOURCES_C:.c=.o) $(SOURCES_ASM:.asm=.o)

# Target utama
all: kernel.bin iso

kernel.bin: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

# Aturan kompilasi C
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Aturan kompilasi assembly
%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

# Bikin ISO image
iso: kernel.bin
	cp kernel.bin iso/boot/kernel
	grub-mkrescue -o oasis.iso iso

# Jalankan di QEMU (langsung boot kernel, gak pake ISO)
run: kernel.bin
	qemu-system-i386 -kernel kernel.bin -drive id=disk0,file=disk.img,format=raw,if=none -device ide-hd,drive=disk0,bus=ide.0 -m 512M

# Bersihin semua file hasil build
clean:
	rm -f $(OBJECTS) kernel.bin oasis.iso

.PHONY: all iso run clean
