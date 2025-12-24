CC=gcc
AS=nasm
LD=ld

CFLAGS=-m32 \
	-ffreestanding \
	-fno-stack-protector \
	-fno-pic \
	-nostdlib \
	-Wall -Wextra \
	-Ikernel

LDFLAGS=-m elf_i386

ASM_SOURCES = boot/entry.asm kernel/interrupt.asm
C_SOURCES = kernel/kernel.c kernel/vga.c kernel/keyboard.c kernel/io.c kernel/string.c kernel/idt.c kernel/pic.c kernel/timer.c kernel/memory.c kernel/paging.c kernel/pmm.c kernel/task.c kernel/tasks_demo.c
ASM_OBJ = $(ASM_SOURCES:.asm=.o)
C_OBJ = $(C_SOURCES:.c=.o)
OBJ = $(ASM_OBJ) $(C_OBJ)

all: iso

kernel.bin: $(OBJ)
	$(LD) $(LDFLAGS) -T kernel/linker.ld -o kernel.bin $(OBJ)

boot/%.o: boot/%.asm
	$(AS) -f elf32 $< -o $@

kernel/%.o: kernel/%.asm
	$(AS) -f elf32 $< -o $@

kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) -c $< -o $@

iso: kernel.bin
	cp kernel.bin iso/boot/
	grub-mkrescue -o oasis.iso iso -d /usr/lib/grub/i386-pc

run: kernel.bin
	qemu-system-i386 -kernel kernel.bin -m 512M

clean:
	rm -f $(OBJ) kernel.bin oasis.iso

.PHONY: all clean run iso


