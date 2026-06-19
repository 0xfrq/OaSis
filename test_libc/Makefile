CC = gcc
CFLAGS = -m32 -nostdlib -fno-builtin -fno-stack-protector -ffreestanding -fno-pie -fno-pic -Wall -Wextra -Iinclude
LD = ld
LDFLAGS = -m elf_i386

# Sumber kode C untuk libc
LIBC_SOURCES = src/libc/stdio.c src/libc/stdlib.c
LIBC_OBJECTS = $(LIBC_SOURCES:.c=.o)

# Test program
TEST_SOURCES = test_printf.c
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)

# Target utama
all: test_printf

# Kompilasi libc
src/libc/%.o: src/libc/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Kompilasi test program
test_printf.o: test_printf.c
	$(CC) $(CFLAGS) -c $< -o $@

# Link test program dengan libc
test_printf: test_printf.o $(LIBC_OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

# Bersihin file hasil build
clean:
	rm -f $(LIBC_OBJECTS) $(TEST_OBJECTS) test_printf

.PHONY: all clean