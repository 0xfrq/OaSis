CC=gcc
LD=ld

CFLAGS=-m32 \
        -ffreestanding \
        -fno-stack-protector \
        -fno-pic \
        -nostdlib \
        -Wall -Wextra

LDFLAGS=-m elf_i386

all: iso

kernel.bin:
	nasm -f elf32 boot/entry.asm -o entry.o
	$(CC) $(CFLAGS) -c kernel/kernel.c -o kernel.o
	$(LD) $(LDFLAGS) -T kernel/linker.ld -o kernel.bin entry.o kernel.o

iso: kernel.bin
	cp kernel.bin iso/boot/
	grub-mkrescue -o oasis.iso iso -d /usr/lib/grub/i386-pc

run:
	qemu-system-i386 -cdrom oasis.iso

clean:
	rm -f *.o *.bin *.iso
